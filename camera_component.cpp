
#include "pch.h"
#include "vr_blockqueue.h"

#include "camera_component.h"





CameraComponent::CameraComponent()
{
	m_frameWidth = 1024;
	m_frameHeight = 1024;

	m_renderTargetWidth = m_frameWidth * 2;
	m_renderTargetHeight = m_frameHeight;
	m_renderTargetBPP = 4;
	m_streamFormat = vr::CVS_FORMAT_RGBX32;
	m_cameraName = "Simulated stereo camera";

	// Intrinsic values in terms of pixels relative to the frame size.
	m_focalX = 450.0f;
	m_focalY = 450.0f;
	m_centerX = 512.0f;
	m_centerY = 512.0f;

	QueryPerformanceCounter(&m_startTime);

	m_currentFrame = std::make_shared<CameraFrameContainer>();
	m_nextFrame = std::make_shared<CameraFrameContainer>();
}

CameraComponent::~CameraComponent()
{
	m_bRunThread = false;
	if (m_frameServeThread.joinable())
	{
		m_frameServeThread.join();
	}
}

bool CameraComponent::Init(vr::TrackedDeviceIndex_t HMDDeviceId)
{
	m_HMDDeviceId = HMDDeviceId;

	const vr::PropertyContainerHandle_t container = vr::VRProperties()->TrackedDeviceToPropertyContainer(m_HMDDeviceId);

	vr::VRProperties()->SetUint64Property(container, vr::Prop_CameraFirmwareVersion_Uint64, 0x200040049); // > 0x200040048
	vr::VRProperties()->SetStringProperty(container, vr::Prop_CameraFirmwareDescription_String, GetCameraDisplayName().c_str());

	vr::VRProperties()->SetBoolProperty(container, vr::Prop_HasCamera_Bool, true);
	vr::VRProperties()->SetBoolProperty(container, vr::Prop_HasCameraComponent_Bool, true);

	vr::VRProperties()->SetInt32Property(container, vr::Prop_NumCameras_Int32, 2);
	vr::VRProperties()->SetInt32Property(container, vr::Prop_CameraFrameLayout_Int32, vr::EVRTrackedCameraFrameLayout_Stereo | vr::EVRTrackedCameraFrameLayout_HorizontalLayout);
	vr::VRProperties()->SetInt32Property(container, vr::Prop_CameraStreamFormat_Int32, m_streamFormat);


	vr::VRProperties()->SetBoolProperty(container, vr::Prop_Hmd_SupportsRoomViewDirect_Bool, false);
	vr::VRProperties()->SetBoolProperty(container, vr::Prop_SupportsRoomViewDepthProjection_Bool, false);

	vr::VRProperties()->SetBoolProperty(container, vr::Prop_CameraSupportsCompatibilityModes_Bool, false);
	vr::VRProperties()->SetInt32Property(container, vr::Prop_CameraCompatibilityMode_Int32, 0);

	vr::VRProperties()->SetBoolProperty(container, vr::Prop_AllowCameraToggle_Bool, true);
	vr::VRProperties()->SetBoolProperty(container, vr::Prop_AllowLightSourceFrequency_Bool, false);

	//vr::VRProperties()->SetFloatProperty(container, vr::Prop_CameraExposureTime_Float, 1.0f);
	//vr::VRProperties()->SetFloatProperty(container, vr::Prop_CameraGlobalGain_Float, 1.0f);

	std::vector<int32_t> CameraDistortionFunction;
	CameraDistortionFunction.resize(2);
	CameraDistortionFunction[0] = (int32_t)vr::VRDistortionFunctionType_Extended_FTheta;
	CameraDistortionFunction[1] = (int32_t)vr::VRDistortionFunctionType_Extended_FTheta;

	vr::VRProperties()->SetPropertyVector(container, vr::Prop_CameraDistortionFunction_Int32_Array, vr::k_unInt32PropertyTag, &CameraDistortionFunction);

	// Note the coefficients being doubles per the comment in the docs
	std::vector<double> CameraDistortionCoeff;
	CameraDistortionCoeff.resize(16);
	CameraDistortionCoeff[0] = 0.0;
	CameraDistortionCoeff[1] = 0.0;
	CameraDistortionCoeff[2] = 0.0;
	CameraDistortionCoeff[3] = 0.0;
	CameraDistortionCoeff[4] = 0.0;
	CameraDistortionCoeff[5] = 0.0;
	CameraDistortionCoeff[6] = 0.0;
	CameraDistortionCoeff[7] = 0.0;

	CameraDistortionCoeff[8] = 0.0;
	CameraDistortionCoeff[9] = 0.0;
	CameraDistortionCoeff[10] = 0.0;
	CameraDistortionCoeff[11] = 0.0;
	CameraDistortionCoeff[12] = 0.0;
	CameraDistortionCoeff[13] = 0.0;
	CameraDistortionCoeff[14] = 0.0;
	CameraDistortionCoeff[15] = 0.0;

	// Needs to be set and retrived with the float property tag despite being doubles.
	vr::VRProperties()->SetPropertyVector(container, vr::Prop_CameraDistortionCoefficients_Float_Array, vr::k_unFloatPropertyTag, &CameraDistortionCoeff);


	std::vector<float> CameraWhiteBalance;
	CameraWhiteBalance.resize(8);
	CameraWhiteBalance[0] = 1.0f;
	CameraWhiteBalance[1] = 1.0f;
	CameraWhiteBalance[2] = 1.0f;

	CameraWhiteBalance[4] = 1.0f;
	CameraWhiteBalance[5] = 1.0f;
	CameraWhiteBalance[6] = 1.0f;

	vr::VRProperties()->SetPropertyVector(container, vr::Prop_CameraWhiteBalance_Vector4_Array, vr::k_unHmdVector4PropertyTag, &CameraWhiteBalance);



	

	std::vector<vr::HmdMatrix34_t> CameraToHeadTransforms;
	CameraToHeadTransforms.resize(2);

	CameraToHeadTransforms[0].m[0][0] = 1;
	CameraToHeadTransforms[0].m[1][1] = 1;
	CameraToHeadTransforms[0].m[2][2] = 1;
	CameraToHeadTransforms[0].m[0][3] = 0.05f;

	CameraToHeadTransforms[1].m[0][0] = 1;
	CameraToHeadTransforms[1].m[1][1] = 1;
	CameraToHeadTransforms[1].m[2][2] = 1;
	CameraToHeadTransforms[1].m[0][3] = -0.05f;

	vr::VRProperties()->SetProperty(container, vr::Prop_CameraToHeadTransform_Matrix34, &CameraToHeadTransforms[0], sizeof(vr::HmdMatrix34_t), vr::k_unHmdMatrix34PropertyTag);
	vr::VRProperties()->SetPropertyVector(container, vr::Prop_CameraToHeadTransforms_Matrix34_Array, vr::k_unHmdMatrix34PropertyTag, &CameraToHeadTransforms);
	


	vr::EBlockQueueError error = vr::VRBlockQueue()->Create(&m_rawFrameQueue, "/lighthouse/camera/raw_frames", m_renderTargetWidth * m_renderTargetHeight * m_renderTargetBPP, 512, 4, 0);
	if (error != vr::EBlockQueueError_BlockQueueError_None)
	{
		VR_DRIVER_LOG_FORMAT("Error creating block queue: {}", (int)error);
		return false;
	}

	int32_t frameFormat = m_streamFormat;
	int32_t frameWidth = m_renderTargetWidth;
	int32_t frameHeight = m_renderTargetHeight;
	uint64_t frameSequence = 0;
	int32_t frameSize = m_renderTargetWidth * m_renderTargetHeight * m_renderTargetBPP;
	double frameTimeMonotonic = 0.0;
	uint64_t serverFrameTicks = 0;
	double deliveryRate = 0.0;
	double elapsedTime = 0.0;

	vr::PathHandle_t formatHandle;
	vr::VRPaths()->StringToHandle(&formatHandle, "/format");

	vr::PathHandle_t widthHandle;
	vr::VRPaths()->StringToHandle(&widthHandle, "/width");

	vr::PathHandle_t heightHandle;
	vr::VRPaths()->StringToHandle(&heightHandle, "/height");

	/*vr::PathHandle_t frameSequenceHandle;
	vr::VRPaths()->StringToHandle(&frameSequenceHandle, "/frame_sequence");

	vr::PathHandle_t frameSizeHandle;
	vr::VRPaths()->StringToHandle(&frameSizeHandle, "/frame_size");

	vr::PathHandle_t frameTimeMonotonicHandle;
	vr::VRPaths()->StringToHandle(&frameTimeMonotonicHandle, "/frame_time_monotonic");

	vr::PathHandle_t serverTimeTicksHandle;
	vr::VRPaths()->StringToHandle(&serverTimeTicksHandle, "/server_time_ticks");

	vr::PathHandle_t deliveryRateHandle;
	vr::VRPaths()->StringToHandle(&deliveryRateHandle, "/delivery_rate");

	vr::PathHandle_t elapsedTimeHandle;
	vr::VRPaths()->StringToHandle(&elapsedTimeHandle, "/elapsed_time");*/


	vr::PathWrite_t write = {};
	write.ulPath = formatHandle;
	write.writeType = vr::PropertyWrite_Set;
	write.unTag = vr::k_unInt32PropertyTag;
	write.unBufferSize = sizeof(frameFormat);
	write.pvBuffer = &frameFormat;
	write.pszPath = nullptr;

	vr::ETrackedPropertyError propError = vr::VRPaths()->WritePathBatch(m_rawFrameQueue, &write, 1);
	if (propError != vr::TrackedProp_Success)
	{
		VR_DRIVER_LOG_FORMAT("Error writing frame format to block queue path: {}", (int)propError);
		return false;
	}

	write.ulPath = widthHandle;
	write.pvBuffer = &frameWidth;

	propError = vr::VRPaths()->WritePathBatch(m_rawFrameQueue, &write, 1);
	if (propError != vr::TrackedProp_Success)
	{
		VR_DRIVER_LOG_FORMAT("Error writing frame width to block queue path: {}", (int)propError);
		return false;
	}

	write.ulPath = heightHandle;
	write.pvBuffer = &frameHeight;

	propError = vr::VRPaths()->WritePathBatch(m_rawFrameQueue, &write, 1);
	if (propError != vr::TrackedProp_Success)
	{
		VR_DRIVER_LOG_FORMAT("Error writing frame height to block queue path: {}", (int)propError);
		return false;
	}

	m_bIsInitialized = true;
	return true;
}

void CameraComponent::Deinit()
{
	m_bRunThread = false;
	if (m_frameServeThread.joinable())
	{
		m_frameServeThread.join();
	}
}


void CameraComponent::ServeFrames()
{
	while (m_bRunThread)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(16));

		if (!m_bRunThread) { break; }

		if (m_bIsStreamPaused) { continue; }

		{
			//std::scoped_lock lock(m_nextFrame->AccessMutex);

			//int bufferIdx = m_frameCount % 2;

			//for (uint32_t y = 0; y < m_renderTargetHeight; y++)
			//{
			//	for (uint32_t x = 0; x < m_renderTargetWidth; x++)
			//	{
			//		/*m_nextFrame->Framebuffer[(x + y * m_renderTargetWidth) * 4] = x % 256;
			//		m_nextFrame->Framebuffer[(x + y * m_renderTargetWidth) * 4 + 1] = y % 256;
			//		m_nextFrame->Framebuffer[(x + y * m_renderTargetWidth) * 4 + 2] = 127;
			//		m_nextFrame->Framebuffer[(x + y * m_renderTargetWidth) * 4 + 3] = 255;*/
			//		m_framebuffers[bufferIdx][(x + y * m_renderTargetWidth) * 4] = x % 256;
			//		m_framebuffers[bufferIdx][(x + y * m_renderTargetWidth) * 4 + 1] = y % 256;
			//		m_framebuffers[bufferIdx][(x + y * m_renderTargetWidth) * 4 + 2] = 127;
			//		m_framebuffers[bufferIdx][(x + y * m_renderTargetWidth) * 4 + 3] = 255;
			//	}
			//}

			m_frameCount++;
			m_frameSequence = (m_frameSequence + 1) % 16;


			vr::PropertyContainerHandle_t writeHandle;
			uint8_t* pBuffer;

			vr::EBlockQueueError error = vr::VRBlockQueue()->AcquireWriteOnlyBlock(m_rawFrameQueue, &writeHandle, (void**)&pBuffer);
			if (error != vr::EBlockQueueError_BlockQueueError_None)
			{
				VR_DRIVER_LOG_FORMAT("AcquireWriteOnlyBlock error: {}", (int)error);
				continue;
			}

			for (uint32_t y = 0; y < m_renderTargetHeight * (4 / m_renderTargetBPP); y++)
			{
				for (uint32_t x = 0; x < m_renderTargetWidth; x++)
				{
					int pixel = (x + y * m_renderTargetWidth) * 4;

					pBuffer[pixel] = ((x * 256) / m_renderTargetWidth * 2) % 256;
					pBuffer[pixel + 1] = ((y * 256) / m_renderTargetHeight) % 256;
					/*if ((m_frameCount % 4 == 0 && x < m_renderTargetWidth / 2 && y < m_renderTargetHeight / 2) ||
						(m_frameCount % 4 == 1 && x >= m_renderTargetWidth / 2 && y < m_renderTargetHeight / 2) || 
						(m_frameCount % 4 == 2 && x >= m_renderTargetWidth / 2 && y >= m_renderTargetHeight / 2) || 
						(m_frameCount % 4 == 3 && x < m_renderTargetWidth / 2 && y >= m_renderTargetHeight / 2))
					{
						pBuffer[pixel+ 2] = 127;
					}
					else
					{
						pBuffer[pixel + 2] = 0;
					}*/
					pBuffer[pixel + 2] = (x < m_renderTargetWidth / 2) ? 127 : 0;					
					pBuffer[pixel + 3] = 255;
	
				}
			}

			

			

			LARGE_INTEGER currTime, perfFrequency;

			QueryPerformanceCounter(&currTime);
			QueryPerformanceFrequency(&perfFrequency);

			int32_t frameSize = m_renderTargetWidth * m_renderTargetHeight * m_renderTargetBPP;

			double latency = 0.040;
			uint64_t latencyTicks = (uint64_t)(latency * (double)perfFrequency.QuadPart);

			uint64_t frameSequence = m_frameSequence;
			double elapsedTime = (currTime.QuadPart - m_startTime.QuadPart) / (double)perfFrequency.QuadPart;
			double frameTimeMonotonic = currTime.QuadPart / (double)perfFrequency.QuadPart - latency;
			uint64_t serverTimeTicks = currTime.QuadPart - latencyTicks;
			double deliveryRate = (double)(currTime.QuadPart - latency - m_lastFrameTime.QuadPart) / (double)perfFrequency.QuadPart;

			m_lastFrameTime.QuadPart = currTime.QuadPart - latency;


			/*m_nextFrame->FrameData.m_nFrameSequence = m_frameSequence;
			m_nextFrame->FrameData.m_nExposureTime = currTime.QuadPart - latency;
			m_nextFrame->FrameData.m_flFrameElapsedTime = elapsedTime;
			m_nextFrame->FrameData.m_flFrameDeliveryRate = deliveryRate;
			m_nextFrame->FrameData.m_flFrameCaptureTime_ServerRelative = frameTimeMonotonic;
			m_nextFrame->FrameData.m_flFrameCaptureTime_DriverAbsolute = serverTimeTicks;
			m_nextFrame->FrameData.m_nFrameCaptureTicks_ServerAbsolute = serverTimeTicks;*/

			vr::PathHandle_t frameSizeHandle;
			vr::VRPaths()->StringToHandle(&frameSizeHandle, "/frame_size");

			vr::PathHandle_t frameSequenceHandle;
			vr::VRPaths()->StringToHandle(&frameSequenceHandle, "/frame_sequence");

			vr::PathHandle_t frameTimeMonotonicHandle;
			vr::VRPaths()->StringToHandle(&frameTimeMonotonicHandle, "/frame_time_monotonic");

			vr::PathHandle_t serverTimeTicksHandle;
			vr::VRPaths()->StringToHandle(&serverTimeTicksHandle, "/server_time_ticks");

			vr::PathHandle_t deliveryRateHandle;
			vr::VRPaths()->StringToHandle(&deliveryRateHandle, "/delivery_rate");

			vr::PathHandle_t elapsedTimeHandle;
			vr::VRPaths()->StringToHandle(&elapsedTimeHandle, "/elapsed_time");

			vr::PathWrite_t write = {};
			write.writeType = vr::PropertyWrite_Set;
			write.pszPath = nullptr;


			write.ulPath = frameSizeHandle;
			write.pvBuffer = &frameSize;
			write.unBufferSize = sizeof(frameSize);
			write.unTag = vr::k_unInt32PropertyTag;

			vr::ETrackedPropertyError propError = vr::VRPaths()->WritePathBatch(writeHandle, &write, 1);
			if (propError != vr::TrackedProp_Success)
			{
				VR_DRIVER_LOG_FORMAT("Error writing frame size to block queue path: {}", (int)propError);
			}

			write.ulPath = frameSequenceHandle;
			write.pvBuffer = &frameSequence;
			write.unBufferSize = sizeof(frameSequence);
			write.unTag = vr::k_unUint64PropertyTag;

			propError = vr::VRPaths()->WritePathBatch(writeHandle, &write, 1);
			if (propError != vr::TrackedProp_Success)
			{
				VR_DRIVER_LOG_FORMAT("Error writing frameSequence to block queue path: {}", (int)propError);
			}


			write.ulPath = frameTimeMonotonicHandle;
			write.pvBuffer = &frameTimeMonotonic;
			write.unBufferSize = sizeof(frameTimeMonotonic);
			write.unTag = vr::k_unDoublePropertyTag;

			propError = vr::VRPaths()->WritePathBatch(writeHandle, &write, 1);
			if (propError != vr::TrackedProp_Success)
			{
				VR_DRIVER_LOG_FORMAT("Error writing frameTimeMonotonic to block queue path: {}", (int)propError);
			}

			write.ulPath = serverTimeTicksHandle;
			write.pvBuffer = &serverTimeTicks;
			write.unBufferSize = sizeof(serverTimeTicks);
			write.unTag = vr::k_unUint64PropertyTag;

			propError = vr::VRPaths()->WritePathBatch(writeHandle, &write, 1);
			if (propError != vr::TrackedProp_Success)
			{
				VR_DRIVER_LOG_FORMAT("Error writing serverTimeTicks to block queue path: {}", (int)propError);
			}

			write.ulPath = deliveryRateHandle;
			write.pvBuffer = &deliveryRate;
			write.unBufferSize = sizeof(deliveryRate);
			write.unTag = vr::k_unDoublePropertyTag;

			propError = vr::VRPaths()->WritePathBatch(writeHandle, &write, 1);
			if (propError != vr::TrackedProp_Success)
			{
				VR_DRIVER_LOG_FORMAT("Error writing deliveryRate to block queue path: {}", (int)propError);
			}

			write.ulPath = elapsedTimeHandle;
			write.pvBuffer = &elapsedTime;
			write.unBufferSize = sizeof(elapsedTime);
			write.unTag = vr::k_unDoublePropertyTag;

			propError = vr::VRPaths()->WritePathBatch(writeHandle, &write, 1);
			if (propError != vr::TrackedProp_Success)
			{
				VR_DRIVER_LOG_FORMAT("Error writing elapsedTime to block queue path: {}", (int)propError);
			}


			error = vr::VRBlockQueue()->ReleaseWriteOnlyBlock(m_rawFrameQueue, writeHandle);
			if (error != vr::EBlockQueueError_BlockQueueError_None)
			{
				VR_DRIVER_LOG_FORMAT("ReleaseWriteOnlyBlock error: {}", (int)error);
				continue;
			}
		}

		/*{
			std::scoped_lock lock(m_frameContainerMutex);
			m_nextFrame.swap(m_currentFrame);
		}*/

		//VR_DRIVER_LOG_FORMAT("Serve: {}", m_frameCount);

		if (m_pCameraVideoSinkCallback != nullptr)
		{
			m_pCameraVideoSinkCallback->OnCameraVideoSinkCallback();
		}

		
	}
}

// Never seems to be called. 
bool CameraComponent::GetCameraFrameDimensions(vr::ECameraVideoStreamFormat nVideoStreamFormat, uint32_t* pWidth, uint32_t* pHeight)
{
	std::string message = std::format("CameraComponent: GetCameraFrameDimensions: {}", (int)nVideoStreamFormat);
	vr::VRDriverLog()->Log(message.c_str());

	*pWidth = m_renderTargetWidth;
	*pHeight = m_renderTargetHeight;

	return true;
}

// Called before video stream started.
bool CameraComponent::GetCameraFrameBufferingRequirements(int* pDefaultFrameQueueSize, uint32_t* pFrameBufferDataSize)
{
	vr::VRDriverLog()->Log("GetCameraFrameBufferingRequirements");
	*pDefaultFrameQueueSize = 4;
	*pFrameBufferDataSize = m_renderTargetWidth * m_renderTargetHeight * m_renderTargetBPP;
	return true;
}


// Called before video stream started. Provides values from GetCameraFrameBufferingRequirements. The Framebuffer pointer s point to allocated data, but are not read in favor for the BlockQueue.
bool CameraComponent::SetCameraFrameBuffering(int nFrameBufferCount, void** ppFrameBuffers, uint32_t nFrameBufferDataSize)
{
	std::string message = std::format("CameraComponent: SetCameraFrameBuffering: count={} dataSize={}", nFrameBufferCount, nFrameBufferDataSize);
	vr::VRDriverLog()->Log(message.c_str());

	if (nFrameBufferCount < 1)
	{
		return false;
	}

	//for (int i = 0; i < nFrameBufferCount; i++)
	//{
	//	m_framebuffers.push_back(static_cast<uint8_t*>(ppFrameBuffers[i]));
	//}

	//vr::HmdMatrix34_t DevicePose = {};

	//DevicePose.m[0][0] = 1;
	//DevicePose.m[1][1] = 1;
	//DevicePose.m[2][2] = 1;
	//DevicePose.m[1][3] = 1.5f;

	//std::scoped_lock lock(m_frameContainerMutex);

	//if (m_currentFrame->AccessMutex.try_lock())
	//{
	//	//m_currentFrame->Framebuffer.resize(nFrameBufferDataSize);
	//	m_currentFrame->Framebuffer = (uint8_t*)ppFrameBuffers[1];
	//	m_currentFrame->FrameData = {};
	//	m_currentFrame->FrameData.m_pImageData = (uint64_t)m_currentFrame->Framebuffer;
	//	m_currentFrame->FrameData.m_nBufferCount = 2;
	//	m_currentFrame->FrameData.m_nBufferIndex = 1;
	//	m_currentFrame->FrameData.m_nWidth= m_renderTargetWidth;
	//	m_currentFrame->FrameData.m_nHeight = m_renderTargetHeight;
	//	m_currentFrame->FrameData.m_nImageDataSize = nFrameBufferDataSize;
	//	m_currentFrame->FrameData.m_nStreamFormat = m_streamFormat;

	//	m_currentFrame->FrameData.m_RawTrackedDevicePose.mDeviceToAbsoluteTracking = DevicePose;
	//	m_currentFrame->FrameData.m_RawTrackedDevicePose.bDeviceIsConnected = true;
	//	m_currentFrame->FrameData.m_RawTrackedDevicePose.bPoseIsValid = true;
	//	m_currentFrame->FrameData.m_RawTrackedDevicePose.eTrackingResult = vr::TrackingResult_Running_OK;

	//	m_currentFrame->AccessMutex.unlock();
	//}

	//if (m_nextFrame->AccessMutex.try_lock())
	//{
	//	m_nextFrame->Framebuffer = (uint8_t*)ppFrameBuffers[0];
	//	m_nextFrame->FrameData = {};
	//	m_nextFrame->FrameData.m_pImageData = (uint64_t)m_currentFrame->Framebuffer;
	//	m_nextFrame->FrameData.m_nBufferCount = 2;
	//	m_nextFrame->FrameData.m_nBufferIndex = 0;
	//	m_nextFrame->FrameData.m_nWidth = m_renderTargetWidth;
	//	m_nextFrame->FrameData.m_nHeight = m_renderTargetHeight;
	//	m_nextFrame->FrameData.m_nImageDataSize = nFrameBufferDataSize;
	//	m_nextFrame->FrameData.m_nStreamFormat = m_streamFormat;

	//	m_nextFrame->FrameData.m_RawTrackedDevicePose.mDeviceToAbsoluteTracking = DevicePose;
	//	m_nextFrame->FrameData.m_RawTrackedDevicePose.bDeviceIsConnected = true;
	//	m_nextFrame->FrameData.m_RawTrackedDevicePose.bPoseIsValid = true;
	//	m_nextFrame->FrameData.m_RawTrackedDevicePose.eTrackingResult = vr::TrackingResult_Running_OK;

	//	m_nextFrame->AccessMutex.unlock();
	//}

	

	return true;
}

// Never seems to be called. 
bool CameraComponent::SetCameraVideoStreamFormat(vr::ECameraVideoStreamFormat nVideoStreamFormat)
{
	std::string message = std::format("CameraComponent: SetCameraVideoStreamFormat: {}", (int)nVideoStreamFormat);
	vr::VRDriverLog()->Log(message.c_str());

	

	return true;
}

// Called before video stream started. Does not seem to use the value.
vr::ECameraVideoStreamFormat CameraComponent::GetCameraVideoStreamFormat()
{
	vr::VRDriverLog()->Log("GetCameraVideoStreamFormat");
	return m_streamFormat;
}


// Called to start the video stream. 
bool CameraComponent::StartVideoStream()
{
	vr::VRDriverLog()->Log("StartVideoStream");

	if (m_bIsStreamActive)
	{
		return true;
	}

	m_bRunThread = false;
	if (m_frameServeThread.joinable())
	{
		m_frameServeThread.join();
	}

	m_bRunThread = true;
	m_frameServeThread = std::thread(&CameraComponent::ServeFrames, this);

	QueryPerformanceCounter(&m_startTime);

	if (m_bIsFirstStart)
	{
		m_bIsFirstStart = false;
		m_firstStartTime = m_startTime;
	}

	m_bIsStreamActive = true;
	m_bIsStreamPaused = false;
	return true;
}

// Never seems to be called. The stream is paused rather than stopped when the camera is not used.
void CameraComponent::StopVideoStream()
{
	vr::VRDriverLog()->Log("StopVideoStream");
	m_bIsStreamActive = false;

	m_bRunThread = false;
	if (m_frameServeThread.joinable())
	{
		m_frameServeThread.join();
	}
}

// Called before a running stream is paused.
bool CameraComponent::IsVideoStreamActive(bool* pbPaused, float* pflElapsedTime)
{
	*pbPaused = m_bIsStreamPaused;

	if (!m_bIsStreamActive)
	{
		return false;
	}

	LARGE_INTEGER currTime, perfFrequency;

	QueryPerformanceCounter(&currTime);
	QueryPerformanceFrequency(&perfFrequency);

	*pflElapsedTime = (currTime.QuadPart - m_startTime.QuadPart) / (float)perfFrequency.QuadPart;
	
	vr::VRDriverLog()->Log("IsVideoStreamActive");
	return true;
}

// Called on OnCameraVideoSinkCallback(). Returning a fame struct seems to reject any frame data passed, and keeps calling the function non-stop. Frame data not used. Seems to be safe to return nullptr.
const vr::CameraVideoStreamFrame_t* CameraComponent::GetVideoStreamFrame()
{
	//VR_DRIVER_LOG_FORMAT("GetVideoStreamFrame: num {}", m_frameCount);
	return nullptr;

	/*if (m_currentFrame == 0)
	{
		vr::VRDriverLog()->Log("GetVideoStreamFrame: No frame yet");
		return nullptr;
	}*/

	//if (m_currentFrame->AccessMutex.try_lock_shared())
	/*{
		std::scoped_lock lock(m_currentFrame->AccessMutex);

		VR_DRIVER_LOG_FORMAT("GetVideoStreamFrame: seq {}, buffer {}", m_currentFrame->FrameData.m_nFrameSequence, m_currentFrame->FrameData.m_nBufferIndex);

		return &(m_currentFrame->FrameData);
	}

	vr::VRDriverLog()->Log("GetVideoStreamFrame: Failed to lock frame");

	return nullptr;*/
}

// Does not seem to be called due to frame data being rejected in the above function.
void CameraComponent::ReleaseVideoStreamFrame(const vr::CameraVideoStreamFrame_t* pFrameImage)
{
	vr::VRDriverLog()->Log("ReleaseVideoStreamFrame");

	//std::scoped_lock lock(m_frameContainerMutex);

	/*if (&(m_currentFrame->FrameData) == pFrameImage)
	{
		m_currentFrame->AccessMutex.unlock_shared();
	}
	else if(&(m_nextFrame->FrameData) == pFrameImage)
	{
		m_nextFrame->AccessMutex.unlock_shared();
	}*/
}

// Never seems to be called. 
bool CameraComponent::SetAutoExposure(bool bEnable)
{
	vr::VRDriverLog()->Log("SetAutoExposure");
	return true;
}

// Called rather than StopVideoStream when no one is using the camera.
bool CameraComponent::PauseVideoStream()
{
	vr::VRDriverLog()->Log("PauseVideoStream");
	m_bIsStreamPaused = true;
	return true;
}

// Called to resume a paused stream.
bool CameraComponent::ResumeVideoStream()
{
	vr::VRDriverLog()->Log("ResumeVideoStream");

	m_bIsStreamPaused = false;
	return true;
}

//static int GetCameraDistortionTimesCalled = 0;

// Called on startup to build distortion mesh. Only used by internal undistortion function.
bool CameraComponent::GetCameraDistortion(uint32_t nCameraIndex, float flInputU, float flInputV, float* pflOutputU, float* pflOutputV)
{
	/*GetCameraDistortionTimesCalled++;

	if (GetCameraDistortionTimesCalled % 10000 == 0)
	{
		VR_DRIVER_LOG_FORMAT("CameraComponent: GetCameraDistortion called {} times, cam {}, [{}, {}]", GetCameraDistortionTimesCalled, nCameraIndex, flInputU, flInputV);
	}*/

	//VR_DRIVER_LOG_FORMAT("CameraComponent: GetCameraDistortion: cam {}, [{}, {}]", nCameraIndex, flInputU, flInputV);

	*pflOutputU = flInputU;
	*pflOutputV = flInputV;

	return true;
}

// Used for undistorted camera projection by both Room View and IVRTrackedCamera.
bool CameraComponent::GetCameraProjection(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, float flZNear, float flZFar, vr::HmdMatrix44_t* pProjection)
{
	std::string message = std::format("CameraComponent: GetCameraProjection: {}, {}, {}", (int)eFrameType, flZNear, flZFar);
	vr::VRDriverLog()->Log(message.c_str());

	memset(pProjection, 0, sizeof(vr::HmdMatrix44_t));

	(*pProjection).m[0][0] = m_focalX / (float)m_frameWidth / 2.0f; // Focal length is relative to the entire rendertarget, which means the hoizontal one needs to be halved for side-by side frames.
	(*pProjection).m[1][1] = m_focalY / (float)m_frameHeight;
	(*pProjection).m[0][2] = m_centerX / (float)m_frameWidth; // The center is even weirder, and the horizontal needs to be 0.5 instead of 0 for a centered FoV.
	(*pProjection).m[1][2] = m_centerY / (float)m_frameHeight - 0.5f; // The vertical is 0 for a centered FoV like a regular projection matrix.
	(*pProjection).m[2][2] = -flZFar / (flZFar - flZNear);
	(*pProjection).m[2][3] = -flZFar * flZNear / (flZFar - flZNear);
	(*pProjection).m[3][2] = -1;


	return true;
}

// Does not seem to be called.
bool CameraComponent::SetFrameRate(int nISPFrameRate, int nSensorFrameRate)
{
	std::string message = std::format("CameraComponent: SetFrameRate: {}, {}", nISPFrameRate, nSensorFrameRate);
	vr::VRDriverLog()->Log(message.c_str());
	return true;
}

// Called before video stream started. Calling back to the function seems to be redundant.
bool CameraComponent::SetCameraVideoSinkCallback(vr::ICameraVideoSinkCallback* pCameraVideoSinkCallback)
{
	m_pCameraVideoSinkCallback = pCameraVideoSinkCallback;
	vr::VRDriverLog()->Log("SetCameraVideoSinkCallback");

	// Requires returning true for room view to start.
	return true;
}

// Does not seem to be called.
bool CameraComponent::GetCameraCompatibilityMode(vr::ECameraCompatibilityMode* pCameraCompatibilityMode)
{
	vr::VRDriverLog()->Log("GetCameraCompatibilityMode");
	return true;
}

// Does not seem to be called.
bool CameraComponent::SetCameraCompatibilityMode(vr::ECameraCompatibilityMode nCameraCompatibilityMode)
{
	std::string message = std::format("CameraComponent: SetCameraCompatibilityMode: {}", (int)nCameraCompatibilityMode);
	vr::VRDriverLog()->Log(message.c_str());

	return true;
}

// Used by both Room View and IVRTrackedCamera (only frame type 2 for Room View).
bool CameraComponent::GetCameraFrameBounds(vr::EVRTrackedCameraFrameType eFrameType, uint32_t* pLeft, uint32_t* pTop, uint32_t* pWidth, uint32_t* pHeight)
{
	std::string message = std::format("CameraComponent: GetCameraFrameBounds: {}", (int)eFrameType);
	vr::VRDriverLog()->Log(message.c_str());

	*pLeft = 0;
	*pTop = 0;
	*pWidth = m_renderTargetWidth;
	*pHeight = m_renderTargetHeight;

	return true;
}

// Used by both Room View and IVRTrackedCamera (only frame type 2 for Room View). The distortion parameters seem unused.
bool CameraComponent::GetCameraIntrinsics(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, vr::HmdVector2_t* pFocalLength, vr::HmdVector2_t* pCenter, vr::EVRDistortionFunctionType* peDistortionType, double rCoefficients[vr::k_unMaxDistortionFunctionParameters])
{
	std::string message = std::format("CameraComponent: GetCameraIntrinsics: {}, {}", nCameraIndex, (int)eFrameType);
	vr::VRDriverLog()->Log(message.c_str());

	(*pFocalLength).v[0] = m_focalX;
	(*pFocalLength).v[1] = m_focalY;

	(*pCenter).v[0] = m_centerX;
	(*pCenter).v[1] = m_centerY;

	/**peDistortionType = vr::VRDistortionFunctionType_Extended_FTheta;

	rCoefficients[0] = 0.0;
	rCoefficients[1] = 0.0;
	rCoefficients[2] = 0.0;
	rCoefficients[3] = 0.0;
	rCoefficients[4] = 0.0;
	rCoefficients[5] = 0.0;
	rCoefficients[6] = 0.0;
	rCoefficients[7] = 0.0;*/

	return true;
}
