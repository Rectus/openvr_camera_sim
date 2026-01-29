
#include "pch.h"
#include "camera_component.h"


CameraComponent::CameraComponent()
{
	m_frameWidth = 1024;
	m_frameHeight = 1024;

	m_textureWidth = m_frameWidth * 2;
	m_textureHeight = m_frameHeight;
	m_textureBPP = 4;
	m_streamFormat = vr::CVS_FORMAT_RGBX32;
	m_cameraName = "Simulated stereo camera";

	// Intrinsic values in terms of pixels relative to the frame size.
	m_focalLeftX = 450.0f;
	m_focalLeftY = 450.0f;
	m_centerLeftX = 512.0f;
	m_centerLeftY = 512.0f;

	m_focalRightX = 450.0f;
	m_focalRightY = 450.0f;
	m_centerRightX = 512.0f;
	m_centerRightY = 512.0f;

	// Extended_FTheta used by Valve Index with 4 radial parameters. Unknown what the diffence between the two variants are.
	m_distortionFunction.resize(2);
	m_distortionFunction[0] = (int32_t)vr::VRDistortionFunctionType_Extended_FTheta;
	m_distortionFunction[1] = (int32_t)vr::VRDistortionFunctionType_Extended_FTheta;

	// Using the Valve Index coeffcients to test with.
	// Note the coefficients being doubles per the comment in the docs
	m_distortionCoeff.resize(16);
	m_distortionCoeff[0] = 0.19;
	m_distortionCoeff[1] = 0.023;
	m_distortionCoeff[2] = -0.19;
	m_distortionCoeff[3] = 0.07;
	m_distortionCoeff[4] = 0.0;
	m_distortionCoeff[5] = 0.0;
	m_distortionCoeff[6] = 0.0;
	m_distortionCoeff[7] = 0.0;

	// Second eye coefficients start at 8
	m_distortionCoeff[8] = 0.19;
	m_distortionCoeff[9] = 0.023;
	m_distortionCoeff[10] = -0.19;
	m_distortionCoeff[11] = 0.07;
	m_distortionCoeff[12] = 0.0;
	m_distortionCoeff[13] = 0.0;
	m_distortionCoeff[14] = 0.0;
	m_distortionCoeff[15] = 0.0;



	QueryPerformanceFrequency(&m_perfCounterFrequency);

	QueryPerformanceCounter(&m_startTime);
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

	// Camera firmware version has to be set above 0x200040048, or the the passthrough fails to start.
	// Note that the HMD and FPGA firmware properties also need to be set (done here in the HMD class). 
	vr::VRProperties()->SetUint64Property(container, vr::Prop_CameraFirmwareVersion_Uint64, 0x200040049); 
	vr::VRProperties()->SetStringProperty(container, vr::Prop_CameraFirmwareDescription_String, GetCameraDisplayName().c_str());

	vr::VRProperties()->SetBoolProperty(container, vr::Prop_HasCamera_Bool, true);
	//vr::VRProperties()->SetBoolProperty(container, vr::Prop_HasCameraComponent_Bool, true); // Should be set by SteamVR.

	// Number of cameras. Header claims up to 4 supported.
	vr::VRProperties()->SetInt32Property(container, vr::Prop_NumCameras_Int32, 2);


	vr::VRProperties()->SetInt32Property(container, vr::Prop_CameraFrameLayout_Int32, vr::EVRTrackedCameraFrameLayout_Stereo | vr::EVRTrackedCameraFrameLayout_HorizontalLayout);

	// This does not seem to affect anything.
	vr::VRProperties()->SetInt32Property(container, vr::Prop_CameraStreamFormat_Int32, m_streamFormat);

	// Adds the appropriate controls to the camera settings menu.
	vr::VRProperties()->SetBoolProperty(container, vr::Prop_AllowCameraToggle_Bool, true);
	vr::VRProperties()->SetBoolProperty(container, vr::Prop_SupportsRoomViewDepthProjection_Bool, false);
	vr::VRProperties()->SetBoolProperty(container, vr::Prop_AllowLightSourceFrequency_Bool, false);

	// Unknown functionality
	vr::VRProperties()->SetBoolProperty(container, vr::Prop_Hmd_SupportsRoomViewDirect_Bool, false);
	vr::VRProperties()->SetBoolProperty(container, vr::Prop_CameraSupportsCompatibilityModes_Bool, false);
	vr::VRProperties()->SetInt32Property(container, vr::Prop_CameraCompatibilityMode_Int32, 0);
	//vr::VRProperties()->SetFloatProperty(container, vr::Prop_CameraExposureTime_Float, 1.0f);
	//vr::VRProperties()->SetFloatProperty(container, vr::Prop_CameraGlobalGain_Float, 1.0f);


	// These two properties are the only way applications can access distortion parameters.
	vr::VRProperties()->SetPropertyVector(container, vr::Prop_CameraDistortionFunction_Int32_Array, vr::k_unInt32PropertyTag, &m_distortionFunction);

	// Needs to be set and retrived with the float property tag despite being doubles.
	// 2x vr::k_unMaxDistortionFunctionParameters since it's for both eyes.
	vr::VRProperties()->SetPropertyVector(container, vr::Prop_CameraDistortionCoefficients_Float_Array, vr::k_unFloatPropertyTag, &m_distortionCoeff);

	// Does not seem to have any effect.
	std::vector<float> CameraWhiteBalance;
	CameraWhiteBalance.resize(8);
	CameraWhiteBalance[0] = 1.0f;
	CameraWhiteBalance[1] = 1.0f;
	CameraWhiteBalance[2] = 1.0f;

	CameraWhiteBalance[4] = 1.0f;
	CameraWhiteBalance[5] = 1.0f;
	CameraWhiteBalance[6] = 1.0f;

	vr::VRProperties()->SetPropertyVector(container, vr::Prop_CameraWhiteBalance_Vector4_Array, vr::k_unHmdVector4PropertyTag, &CameraWhiteBalance);



	
	// Inverse poses of cameras relative to the HMD origin.
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
	

	// Create the block queue to serve frames to. Unknown if other values for header and block count works.
	vr::EBlockQueueError error = vr::VRBlockQueue()->Create(&m_rawFrameQueue, "/lighthouse/camera/raw_frames", m_textureWidth * m_textureHeight * m_textureBPP, 512, 4, 0);
	if (error != vr::EBlockQueueError_BlockQueueError_None)
	{
		VR_DRIVER_LOG_FORMAT("Error creating block queue: {}", (int)error);
		return false;
	}

	int32_t frameFormat = m_streamFormat;
	int32_t frameWidth = m_textureWidth;
	int32_t frameHeight = m_textureHeight;

	// Texture format of framebuffer
	vr::PathHandle_t formatHandle;
	vr::VRPaths()->StringToHandle(&formatHandle, "/format");

	vr::PathHandle_t widthHandle;
	vr::VRPaths()->StringToHandle(&widthHandle, "/width");

	vr::PathHandle_t heightHandle;
	vr::VRPaths()->StringToHandle(&heightHandle, "/height");

	vr::VRPaths()->StringToHandle(&m_frameSequenceHandle, "/frame_sequence");
	vr::VRPaths()->StringToHandle(&m_frameSizeHandle, "/frame_size");
	vr::VRPaths()->StringToHandle(&m_frameTimeMonotonicHandle, "/frame_time_monotonic");
	vr::VRPaths()->StringToHandle(&m_serverTimeTicksHandle, "/server_time_ticks");
	vr::VRPaths()->StringToHandle(&m_deliveryRateHandle, "/delivery_rate");
	vr::VRPaths()->StringToHandle(&m_elapsedTimeHandle, "/elapsed_time");


	vr::PathWrite_t write = {};
	write.ulPath = formatHandle;
	write.writeType = vr::PropertyWrite_Set;
	write.unTag = vr::k_unInt32PropertyTag;
	write.unBufferSize = sizeof(frameFormat);
	write.pvBuffer = &frameFormat;
	write.pszPath = nullptr;

	// The format and frame dimensions are written directly using the raw_frames block queue handle.
	// These don't like being written all in a single batch for some reason.
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

// Thread that runs loop serving frames while the video stream is enabled by the runtime.
void CameraComponent::ServeFrames()
{
	while (m_bRunThread)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(16));

		if (!m_bRunThread) { break; }

		if (m_bIsStreamPaused) { continue; }


		m_frameCount++;
		m_frameSequence = (m_frameSequence + 1) % 16;


		vr::PropertyContainerHandle_t writeHandle;
		uint8_t* pBuffer;

		vr::EBlockQueueError error = vr::VRBlockQueue()->AcquireWriteOnlyBlock(m_rawFrameQueue, &writeHandle, (void**)&pBuffer);
		if (error != vr::EBlockQueueError_BlockQueueError_None)
		{
			std::string info = std::format("AcquireWriteOnlyBlock error: {}", (int)error);
			vr::VRDriverLog()->Log(info.c_str());
			continue;
		}

		// Draw image to framebuffer
		for (uint32_t y = 0; y < m_textureHeight; y++)
		{
			for (uint32_t x = 0; x < m_textureWidth; x++)
			{
				int pixel = (x + y * m_textureWidth) * 4;


				// Draw grid lines
				if (y % 64 == (m_textureHeight / 2) % 64 || x % 64 == (m_textureWidth / 4) % 64)
				{
					pBuffer[pixel] = 160;
					pBuffer[pixel + 1] = 160;
					pBuffer[pixel + 2] = 160;
					pBuffer[pixel + 3] = 255;
				}
				else
				{
					pBuffer[pixel] = ((x * 256) / m_textureWidth * 2) % 256;
					pBuffer[pixel + 1] = ((y * 256) / m_textureHeight) % 256;
					pBuffer[pixel + 2] = (x < m_textureWidth / 2) ? 127 : 0; // Tint left view blue
					pBuffer[pixel + 3] = 255;
				}
			}
		}
		
		LARGE_INTEGER currTime;

		QueryPerformanceCounter(&currTime);

		int32_t frameSize = m_textureWidth * m_textureHeight * m_textureBPP;

		double latency = 0.040;
		uint64_t latencyTicks = (uint64_t)(latency * (double)m_perfCounterFrequency.QuadPart);

		uint64_t frameSequence = m_frameSequence;
		double elapsedTime = (currTime.QuadPart - m_startTime.QuadPart) / (double)m_perfCounterFrequency.QuadPart;
		double frameTimeMonotonic = currTime.QuadPart / (double)m_perfCounterFrequency.QuadPart - latency;
		uint64_t serverTimeTicks = currTime.QuadPart - latencyTicks;
		double deliveryRate = (double)(currTime.QuadPart - latency - m_lastFrameTime.QuadPart) / (double)m_perfCounterFrequency.QuadPart;

		m_lastFrameTime.QuadPart = currTime.QuadPart - latencyTicks;

		// Write the per-frame metadata to the handle recived by AcquireWriteOnlyBlock.
		std::vector<vr::PathWrite_t> write(6, {0});

		write[0].writeType = vr::PropertyWrite_Set;
		write[0].ulPath = m_frameSizeHandle;
		write[0].pvBuffer = &frameSize;
		write[0].unBufferSize = sizeof(frameSize);
		write[0].unTag = vr::k_unInt32PropertyTag;

		write[1].writeType = vr::PropertyWrite_Set;
		write[1].ulPath = m_frameSequenceHandle;
		write[1].pvBuffer = &frameSequence;
		write[1].unBufferSize = sizeof(frameSequence);
		write[1].unTag = vr::k_unUint64PropertyTag;

		write[2].writeType = vr::PropertyWrite_Set;
		write[2].ulPath = m_frameTimeMonotonicHandle;
		write[2].pvBuffer = &frameTimeMonotonic;
		write[2].unBufferSize = sizeof(frameTimeMonotonic);
		write[2].unTag = vr::k_unDoublePropertyTag;

		write[3].writeType = vr::PropertyWrite_Set;
		write[3].ulPath = m_serverTimeTicksHandle;
		write[3].pvBuffer = &serverTimeTicks;
		write[3].unBufferSize = sizeof(serverTimeTicks);
		write[3].unTag = vr::k_unUint64PropertyTag;

		write[4].writeType = vr::PropertyWrite_Set;
		write[4].ulPath = m_deliveryRateHandle;
		write[4].pvBuffer = &deliveryRate;
		write[4].unBufferSize = sizeof(deliveryRate);
		write[4].unTag = vr::k_unDoublePropertyTag;

		write[5].writeType = vr::PropertyWrite_Set;
		write[5].ulPath = m_elapsedTimeHandle;
		write[5].pvBuffer = &elapsedTime;
		write[5].unBufferSize = sizeof(elapsedTime);
		write[5].unTag = vr::k_unDoublePropertyTag;

		vr::ETrackedPropertyError propError = vr::VRPaths()->WritePathBatch(writeHandle, write.data(), 6);
		if (propError != vr::TrackedProp_Success)
		{
			VR_DRIVER_LOG_FORMAT("Error writing frame data to block queue path: {}", (int)propError);
		}

		error = vr::VRBlockQueue()->ReleaseWriteOnlyBlock(m_rawFrameQueue, writeHandle);
		if (error != vr::EBlockQueueError_BlockQueueError_None)
		{
			VR_DRIVER_LOG_FORMAT("ReleaseWriteOnlyBlock error: {}", (int)error);
			continue;
		}

		//VR_DRIVER_LOG_FORMAT("Serve: {}", m_frameCount);

		// It doesn't seem to be required to call this.
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

	*pWidth = m_textureWidth;
	*pHeight = m_textureHeight;

	return true;
}

// Called before video stream started.
bool CameraComponent::GetCameraFrameBufferingRequirements(int* pDefaultFrameQueueSize, uint32_t* pFrameBufferDataSize)
{
	vr::VRDriverLog()->Log("GetCameraFrameBufferingRequirements");
	*pDefaultFrameQueueSize = 4;
	*pFrameBufferDataSize = m_textureWidth * m_textureHeight * m_textureBPP;
	return true;
}


// Called before video stream started. Provides values from GetCameraFrameBufferingRequirements. The Framebuffer pointers point to allocated data, but are not read from in favor for the BlockQueue.
bool CameraComponent::SetCameraFrameBuffering(int nFrameBufferCount, void** ppFrameBuffers, uint32_t nFrameBufferDataSize)
{
	std::string message = std::format("CameraComponent: SetCameraFrameBuffering: count={} dataSize={}", nFrameBufferCount, nFrameBufferDataSize);
	vr::VRDriverLog()->Log(message.c_str());

	if (nFrameBufferCount < 1)
	{
		return false;
	}

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

	LARGE_INTEGER currTime;

	QueryPerformanceCounter(&currTime);

	*pflElapsedTime = (currTime.QuadPart - m_startTime.QuadPart) / (float)m_perfCounterFrequency.QuadPart;
	
	vr::VRDriverLog()->Log("IsVideoStreamActive");
	return true;
}

// Called on OnCameraVideoSinkCallback(). Returning a frame struct seems to reject any frame data passed, and keeps calling the function non-stop. Frame data not used. Seems to be safe to return nullptr.
const vr::CameraVideoStreamFrame_t* CameraComponent::GetVideoStreamFrame()
{
	//VR_DRIVER_LOG_FORMAT("GetVideoStreamFrame: num {}", m_frameCount);
	return nullptr;
}

// Does not seem to be called due to frame data being rejected in the above function.
void CameraComponent::ReleaseVideoStreamFrame(const vr::CameraVideoStreamFrame_t* pFrameImage)
{
	vr::VRDriverLog()->Log("ReleaseVideoStreamFrame");
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

// Called on startup to build distortion mesh. Only used by internal undistortion to generate Room View (directly) and EVRTrackedCameraFrameType 1 and 2.
bool CameraComponent::GetCameraDistortion(uint32_t nCameraIndex, float flInputU, float flInputV, float* pflOutputU, float* pflOutputV)
{
	/*GetCameraDistortionTimesCalled++;

	if (GetCameraDistortionTimesCalled % 10000 == 0)
	{
		VR_DRIVER_LOG_FORMAT("CameraComponent: GetCameraDistortion called {} times, cam {}, [{}, {}]", GetCameraDistortionTimesCalled, nCameraIndex, flInputU, flInputV);
	}*/

	//VR_DRIVER_LOG_FORMAT("CameraComponent: GetCameraDistortion: cam {}, [{}, {}]", nCameraIndex, flInputU, flInputV);


	// Radial fisheye lens distortion correction as described here: https://docs.opencv.org/4.x/db/d58/group__calib3d__fisheye.html

	double focalX;
	double focalY;

	double centerX;
	double centerY;

	int distIndex = (nCameraIndex == 0) ? 0 : 8;

	if (nCameraIndex == 0)
	{
		focalX = m_focalLeftX / (double)m_frameWidth;
		focalY = m_focalLeftY / (double)m_frameHeight;

		centerX = m_centerLeftX / (double)m_frameWidth - 0.5;
		centerY = m_centerLeftY / (double)m_frameHeight - 0.5;
	}
	else
	{
		focalX = m_focalRightX / (double)m_frameWidth;
		focalY = m_focalRightY / (double)m_frameHeight;

		centerX = m_centerRightX / (double)m_frameWidth - 0.5;
		centerY = m_centerRightY / (double)m_frameHeight - 0.5;
	}

	double UScaled = (flInputU - 0.5) * 2.0 / focalX;
	double VScaled = (flInputV - 0.5) * 2.0 / focalY;

	double radius = sqrt(UScaled * UScaled + VScaled * VScaled);

	double theta = atan(radius);

	double thetaD = theta +
		m_distortionCoeff[distIndex + 0] * pow(theta, 3) +
		m_distortionCoeff[distIndex + 1] * pow(theta, 5) +
		m_distortionCoeff[distIndex + 2] * pow(theta, 7) +
		m_distortionCoeff[distIndex + 3] * pow(theta, 9);

	double radialFactor = thetaD / radius;

	*pflOutputU = (float)(UScaled * radialFactor * focalX + centerX + 0.5);
	*pflOutputV = (float)(VScaled * radialFactor * focalY + centerY + 0.5);

	return true;
}

// Used for undistorted camera projection by both Room View and IVRTrackedCamera.
bool CameraComponent::GetCameraProjection(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, float flZNear, float flZFar, vr::HmdMatrix44_t* pProjection)
{
	std::string message = std::format("CameraComponent: GetCameraProjection: {}, {}, {}, {}", nCameraIndex, (int)eFrameType, flZNear, flZFar);
	vr::VRDriverLog()->Log(message.c_str());


	float focalX = (nCameraIndex == 0) ? m_focalLeftX : m_focalRightX;
	float focalY = (nCameraIndex == 0) ? m_focalLeftY : m_focalRightY;

	float centerX = (nCameraIndex == 0) ? m_centerLeftX : m_centerRightX;
	float centerY = (nCameraIndex == 0) ? m_centerLeftY : m_centerRightY;


	memset(pProjection, 0, sizeof(vr::HmdMatrix44_t));

	(*pProjection).m[0][0] = focalX / (float)m_frameWidth / 2.0f; // Focal length is relative to the entire rendertarget, which means the hoizontal one needs to be halved for side-by side frames.
	(*pProjection).m[1][1] = focalY / (float)m_frameHeight;
	(*pProjection).m[0][2] = centerX / (float)m_frameWidth; // The center is even weirder, and the horizontal needs to be 0.5 instead of 0 for a centered FoV.
	(*pProjection).m[1][2] = centerY / (float)m_frameHeight - 0.5f; // The vertical is 0 for a centered FoV like a regular projection matrix.
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
	*pWidth = m_textureWidth;
	*pHeight = m_textureHeight;

	return true;
}

// Used by both Room View and IVRTrackedCamera (only frame type 2 for Room View). The distortion parameters seem unused.
bool CameraComponent::GetCameraIntrinsics(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, vr::HmdVector2_t* pFocalLength, vr::HmdVector2_t* pCenter, vr::EVRDistortionFunctionType* peDistortionType, double rCoefficients[vr::k_unMaxDistortionFunctionParameters])
{
	std::string message = std::format("CameraComponent: GetCameraIntrinsics: {}, {}", nCameraIndex, (int)eFrameType);
	vr::VRDriverLog()->Log(message.c_str());

	(*pFocalLength).v[0] = (nCameraIndex == 0) ? m_focalLeftX : m_focalRightX;
	(*pFocalLength).v[1] = (nCameraIndex == 0) ? m_focalLeftY : m_focalRightY;

	(*pCenter).v[0] = (nCameraIndex == 0) ? m_centerLeftX : m_centerRightX;
	(*pCenter).v[1] = (nCameraIndex == 0) ? m_centerLeftY : m_centerRightY;


	// Unknown if the below parameters are used or accessible anywhere.
	int distIndex = (nCameraIndex == 0) ? 0 : 8;

	*peDistortionType = (vr::EVRDistortionFunctionType)m_distortionFunction[nCameraIndex % 2];

	rCoefficients[0] = m_distortionCoeff[distIndex + 0];
	rCoefficients[1] = m_distortionCoeff[distIndex + 1];
	rCoefficients[2] = m_distortionCoeff[distIndex + 2];
	rCoefficients[3] = m_distortionCoeff[distIndex + 3];
	rCoefficients[4] = m_distortionCoeff[distIndex + 4];
	rCoefficients[5] = m_distortionCoeff[distIndex + 5];
	rCoefficients[6] = m_distortionCoeff[distIndex + 6];
	rCoefficients[7] = m_distortionCoeff[distIndex + 7];

	return true;
}
