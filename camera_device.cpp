
#include "pch.h"
#include "camera_device.h"



#define FRAME_RATE 60;
#define DISPLAY_CONFIG "openvr_camera_sim_display"

CameraDevice::CameraDevice()
	: m_deviceId(-1)
{
	m_cameraComponent = std::make_unique<CameraComponent>();
	//m_displayComponent = std::make_unique<CameraDisplayComponent>();

	m_windowPosX = vr::VRSettings()->GetInt32(DISPLAY_CONFIG, "window_x");
	m_windowPosY = vr::VRSettings()->GetInt32(DISPLAY_CONFIG, "window_y");

	m_windowWidth = vr::VRSettings()->GetInt32(DISPLAY_CONFIG, "window_width");
	m_windowHeight = vr::VRSettings()->GetInt32(DISPLAY_CONFIG, "window_height");

	m_renderWidth = vr::VRSettings()->GetInt32(DISPLAY_CONFIG, "render_width");
	m_renderHeight = vr::VRSettings()->GetInt32(DISPLAY_CONFIG, "render_height");

	QueryPerformanceCounter(&m_lastVsync);
};

vr::EVRInitError CameraDevice::Activate(uint32_t unObjectId) 
{

	std::string message = std::format("CameraDevice::Activate: {}", unObjectId);
	vr::VRDriverLog()->Log(message.c_str());

	m_deviceId = unObjectId;

	m_window = std::make_unique<DisplayWindow>(m_windowPosX, m_windowPosY, m_windowWidth, m_windowHeight);

	if (!m_window->IsRunning())
	{
		vr::VRDriverLog()->Log("Display Window failed to start!");
		return vr::VRInitError_Driver_Failed;
	}

	while (!m_window->IsInitialized())
	{
		if (!m_window->IsRunning())
		{
			vr::VRDriverLog()->Log("Display Window failed to start!");
			return vr::VRInitError_Driver_Failed;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	m_renderer = std::make_shared<D3D11Renderer>(m_window->GetWindow());
	

	if (!m_renderer->InitRenderer())
	{
		vr::VRDriverLog()->Log("Renderer failed!");
		return vr::VRInitError_Driver_Failed;
	}

	if (!m_cameraComponent->Init(m_deviceId))
	{
		vr::VRDriverLog()->Log("Camera component failed!");
		return vr::VRInitError_Driver_Failed;
	}

	const vr::PropertyContainerHandle_t container = vr::VRProperties()->TrackedDeviceToPropertyContainer(unObjectId);

	vr::VRProperties()->SetStringProperty(container, vr::Prop_ModelNumber_String, "openvr_camera_sim_virtual_display");

	vr::VRProperties()->SetUint64Property(container, vr::Prop_FirmwareVersion_Uint64, 0x56456bA0); // > 0x56456b9f
	vr::VRProperties()->SetUint64Property(container, vr::Prop_FPGAVersion_Uint64, 0x104); // > 0x103

	const float ipd = vr::VRSettings()->GetFloat(vr::k_pch_SteamVR_Section, vr::k_pch_SteamVR_IPD_Float);
	vr::VRProperties()->SetFloatProperty(container, vr::Prop_UserIpdMeters_Float, ipd);

	vr::VRProperties()->SetBoolProperty(container, vr::Prop_DisplayDebugMode_Bool, true);
	vr::VRProperties()->SetFloatProperty(container, vr::Prop_SecondsFromVsyncToPhotons_Float, 0.0f);
	vr::VRProperties()->SetFloatProperty(container, vr::Prop_UserHeadToEyeDepthMeters_Float, 0.0f);
	vr::VRProperties()->SetUint64Property(container, vr::Prop_GraphicsAdapterLuid_Uint64, m_renderer->GetLUID().uintLUID);
	vr::VRProperties()->SetFloatProperty(container, vr::Prop_DisplayFrequency_Float, 0.f);
	vr::VRProperties()->SetBoolProperty(container, vr::Prop_IsOnDesktop_Bool, false);
	vr::VRProperties()->SetBoolProperty(container, vr::Prop_ContainsProximitySensor_Bool, false);
	vr::VRProperties()->SetBoolProperty(container, vr::Prop_IgnoreMotionForStandby_Bool, true);

	vr::VRProperties()->SetUint64Property(container, vr::Prop_CurrentUniverseId_Uint64, 42069);
	vr::VRProperties()->SetBoolProperty(container, vr::Prop_NeverTracked_Bool, true);

	std::vector<vr::HmdMatrix34_t> EyeToHeadTransforms;
	EyeToHeadTransforms.resize(2);

	EyeToHeadTransforms[0].m[0][0] = 1;
	EyeToHeadTransforms[0].m[1][1] = 1;
	EyeToHeadTransforms[0].m[2][2] = 1;
	EyeToHeadTransforms[0].m[0][3] = 0.04f;

	EyeToHeadTransforms[1].m[0][0] = 1;
	EyeToHeadTransforms[1].m[1][1] = 1;
	EyeToHeadTransforms[1].m[2][2] = 1;
	EyeToHeadTransforms[1].m[0][3] = -0.04f;

	vr::VRServerDriverHost()->SetDisplayEyeToHead(vr::k_unTrackedDeviceIndex_Hmd, EyeToHeadTransforms[0], EyeToHeadTransforms[1]);


	vr::VRProperties()->SetStringProperty(container, vr::Prop_InputProfilePath_String, "{simplehmd}/input/simulated_hmd_profile.json");

	vr::VRDriverInput()->CreateBooleanComponent(container, "/input/system/touch", &m_inputHandles[0]);
	vr::VRDriverInput()->CreateBooleanComponent(container, "/input/system/click", &m_inputHandles[1]);

	m_bRunThread = true;
	m_poseThread = std::thread(&CameraDevice::RunPoseThread, this);
	
	return vr::VRInitError_None;
}

void CameraDevice::RunPoseThread()
{
	std::this_thread::sleep_for(std::chrono::milliseconds(1));

	while (m_bRunThread)
	{
		vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_deviceId, GetPose(), sizeof(vr::DriverPose_t));
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
}

void CameraDevice::RunFrame() 
{
	m_frameNumber++;

}

void CameraDevice::HandleEvent(const vr::VREvent_t& vrevent) 
{
	if (vrevent.eventType >= 1500 && vrevent.eventType < 1600)
	{
		std::string message = std::format("Camera event: {}", vrevent.eventType);
		vr::VRDriverLog()->Log(message.c_str());
	}
	switch (vrevent.eventType) 
	{
	case vr::VREvent_CameraSettingsHaveChanged:
	{

		vr::VRDriverLog()->Log("VREvent_CameraSettingsHaveChanged");
		break;
	}
	}
}

bool CameraDevice::IsDisplayOnDesktop()
{
	return false;
}

bool CameraDevice::IsDisplayRealDisplay()
{
	return false;
}

void CameraDevice::GetRecommendedRenderTargetSize(uint32_t* pnWidth, uint32_t* pnHeight)
{
	*pnWidth = m_renderWidth;
	*pnHeight = m_renderHeight;
}

void CameraDevice::GetEyeOutputViewport(vr::EVREye eEye, uint32_t* pnX, uint32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight)
{
	*pnX = (eEye == vr::Eye_Left) ? 0 : m_renderWidth;
	*pnY = 0;

	*pnWidth = m_renderWidth;
	*pnHeight = m_renderHeight;
}

void CameraDevice::GetProjectionRaw(vr::EVREye eEye, float* pfLeft, float* pfRight, float* pfTop, float* pfBottom)
{
	*pfLeft = -1.0;
	*pfRight = 1.0;
	*pfTop = -1.0;
	*pfBottom = 1.0;
}

vr::DistortionCoordinates_t CameraDevice::ComputeDistortion(vr::EVREye eEye, float fU, float fV)
{
	vr::DistortionCoordinates_t coordinates{};
	coordinates.rfBlue[0] = fU;
	coordinates.rfBlue[1] = fV;
	coordinates.rfGreen[0] = fU;
	coordinates.rfGreen[1] = fV;
	coordinates.rfRed[0] = fU;
	coordinates.rfRed[1] = fV;
	return coordinates;
}

void CameraDevice::GetWindowBounds(int32_t* pnX, int32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight)
{
	*pnX = 0;
	*pnY = 0;

	// These get used for the compositor output buffer dimensions under IVRVirtualDisplay for some reason.
	*pnWidth = m_renderWidth * 2;
	*pnHeight = m_renderHeight;
}

bool CameraDevice::ComputeInverseDistortion(vr::HmdVector2_t* pResult, vr::EVREye eEye, uint32_t unChannel, float fU, float fV)
{
	return false;
}

void CameraDevice::Deactivate() 
{
	vr::VRDriverLog()->Log("Deactivate");

	if (m_bRunThread.exchange(false))
	{
		m_poseThread.join();
	}
}

void CameraDevice::EnterStandby() 
{
	vr::VRDriverLog()->Log("EnterStandby");
}

void* CameraDevice::GetComponent(const char* pchComponentNameAndVersion) 
{
	if (strcmp(pchComponentNameAndVersion, vr::IVRCameraComponent_Version) == 0)
	{
		vr::VRDriverLog()->Log("GetComponent: IVRCameraComponent");
		return m_cameraComponent.get();
	}
	else if (strcmp(pchComponentNameAndVersion, vr::IVRDisplayComponent_Version) == 0)
	{
		vr::VRDriverLog()->Log("GetComponent: IVRDisplayComponent");
		//return m_displayComponent.get();
		return static_cast<vr::IVRDisplayComponent*>(this);
	}
	else if (strcmp(pchComponentNameAndVersion, vr::IVRVirtualDisplay_Version) == 0)
	{
		vr::VRDriverLog()->Log("GetComponent: IVRVirtualDisplay");
		//return m_displayComponent.get();
		return static_cast<vr::IVRVirtualDisplay*>(this);
	}
	/*else if (strcmp(pchComponentNameAndVersion, vr::IVRDriverDirectModeComponent_Version) == 0)
	{
		vr::VRDriverLog()->Log("GetComponent: IVRDriverDirectModeComponent");
		return m_displayComponent.get();
	}*/
	/*else if (strcmp(pchComponentNameAndVersion, vr::IVRVirtualDisplay_Version) == 0)
	{
		vr::VRDriverLog()->Log("GetComponent: IVRVirtualDisplay");
		return m_displayComponent.get();
	}*/
	else
	{
		std::string message = std::format("GetComponent Unhandled: {}", pchComponentNameAndVersion);
		vr::VRDriverLog()->Log(message.c_str());
	}

	return nullptr;
}

void CameraDevice::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) 
{
	VR_DRIVER_LOG_FORMAT("DebugRequest: {} {}", pchRequest, unResponseBufferSize);
	if (unResponseBufferSize >= 1)
	{
		pchResponseBuffer[0] = 0;
	}
}

// 3x3 or 3x4 matrix
template < class T >
vr::HmdQuaternion_t HmdQuaternion_FromMatrix(const T& matrix)
{
	vr::HmdQuaternion_t q{};

	q.w = sqrt(fmax(0, 1 + matrix.m[0][0] + matrix.m[1][1] + matrix.m[2][2])) / 2;
	q.x = sqrt(fmax(0, 1 + matrix.m[0][0] - matrix.m[1][1] - matrix.m[2][2])) / 2;
	q.y = sqrt(fmax(0, 1 - matrix.m[0][0] + matrix.m[1][1] - matrix.m[2][2])) / 2;
	q.z = sqrt(fmax(0, 1 - matrix.m[0][0] - matrix.m[1][1] + matrix.m[2][2])) / 2;

	q.x = copysign(q.x, matrix.m[2][1] - matrix.m[1][2]);
	q.y = copysign(q.y, matrix.m[0][2] - matrix.m[2][0]);
	q.z = copysign(q.z, matrix.m[1][0] - matrix.m[0][1]);

	return q;
}

vr::DriverPose_t CameraDevice::GetPose() 
{
	vr::DriverPose_t pose = { 0 };

	pose.poseIsValid = true;
	pose.result = vr::TrackingResult_Running_OK;
	pose.deviceIsConnected = true;
	pose.shouldApplyHeadModel = true;

	pose.qWorldFromDriverRotation.w = 1.f;
	pose.qDriverFromHeadRotation.w = 1.f;

	//pose.qRotation.w = fmod(m_frameCount * 0.0001, 2.0) - 1.0;
	//pose.qRotation.y = sqrt(1.0 - pose.qRotation.w * pose.qRotation.w);
	pose.qRotation.w = sin(m_frameCount * 0.0001);
	pose.qRotation.y = cos(m_frameCount * 0.0001);
	//pose.qRotation.w = 1.0;
	//pose.qRotation.y = 0.0;

	pose.vecPosition[1] = 1.5;
	
	pose.vecAngularVelocity[1] = -0.001;

	return pose;
}

void CameraDevice::Present(const vr::PresentInfo_t* pPresentInfo, uint32_t unPresentInfoSize)
{
	//vr::VRDriverLog()->Log("Present()");

	//std::string info = std::format("VSyncTime: {}, FrameId: {}, Vsync: {}, size: {}", pPresentInfo->flVSyncTimeInSeconds, pPresentInfo->nFrameId, (int)pPresentInfo->vsync, unPresentInfoSize);
	//vr::VRDriverLog()->Log(info.c_str());

	m_bWaitForVSync = pPresentInfo->vsync == vr::EVSync::VSync_WaitRender;

	m_renderer->Render(pPresentInfo, m_bWaitForVSync && m_bHasVSyncTime);
}

void CameraDevice::WaitForPresent()
{
	//vr::VRDriverLog()->Log("WaitForPresent()");

	//Sleep(10);
	m_renderer->FinishRender();
	
	if (!m_bHasVSyncTime || m_bWaitForVSync)
	{
		m_bHasVSyncTime = true;
		QueryPerformanceCounter(&m_lastVsync);
		m_frameCount++;
	}
	else
	{
		LARGE_INTEGER perfFrequency, currTime;
		QueryPerformanceFrequency(&perfFrequency);

		QueryPerformanceCounter(&currTime);

		uint64_t frameIntervalTicks = perfFrequency.QuadPart / FRAME_RATE;
		int framesElaped = max((currTime.QuadPart - m_lastVsync.QuadPart) / frameIntervalTicks, 0);
		m_lastVsync.QuadPart += framesElaped * frameIntervalTicks;
		m_frameCount += framesElaped;
	}

}

bool CameraDevice::GetTimeSinceLastVsync(float* pfSecondsSinceLastVsync, uint64_t* pulFrameCounter)
{
	//vr::VRDriverLog()->Log("GetTimeSinceLastVsync()");

	LARGE_INTEGER currTime, perfFrequency;

	QueryPerformanceCounter(&currTime);
	QueryPerformanceFrequency(&perfFrequency);

	*pfSecondsSinceLastVsync = (currTime.QuadPart - m_lastVsync.QuadPart) / (float)perfFrequency.QuadPart;
	*pulFrameCounter = m_frameCount;

	//std::string info = std::format("pfSecondsSinceLastVsync: {}", *pfSecondsSinceLastVsync);
	//vr::VRDriverLog()->Log(info.c_str());

	return false;
}

