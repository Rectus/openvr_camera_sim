
#pragma once

#include "camera_component.h"
#include "d3d11_renderer.h"
#include "display_window.h"

// What the docs don't tell you is that you need both IVRDisplayComponent and IVRVirtualDisplay to use the latter.
class CameraDevice : public vr::ITrackedDeviceServerDriver, public vr::IVRDisplayComponent, public vr::IVRVirtualDisplay
{
public:
	CameraDevice();

	virtual vr::EVRInitError Activate(uint32_t unObjectId) override;
	virtual void Deactivate() override;
	virtual void EnterStandby() override;
	virtual void* GetComponent(const char* pchComponentNameAndVersion) override;
	virtual void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) override;
	virtual vr::DriverPose_t GetPose() override;

	void RunPoseThread();
	void RunFrame();
	void HandleEvent(const vr::VREvent_t& vrevent);

	//vr::IVRDisplayComponent
	bool IsDisplayOnDesktop() override;
	bool IsDisplayRealDisplay() override;
	void GetRecommendedRenderTargetSize(uint32_t* pnWidth, uint32_t* pnHeight) override;
	void GetEyeOutputViewport(vr::EVREye eEye, uint32_t* pnX, uint32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight) override;
	void GetProjectionRaw(vr::EVREye eEye, float* pfLeft, float* pfRight, float* pfTop, float* pfBottom) override;
	vr::DistortionCoordinates_t ComputeDistortion(vr::EVREye eEye, float fU, float fV) override;
	void GetWindowBounds(int32_t* pnX, int32_t* pnY, uint32_t* pnWidth, uint32_t* pnHeight) override;
	bool ComputeInverseDistortion(vr::HmdVector2_t* pResult, vr::EVREye eEye, uint32_t unChannel, float fU, float fV) override;

	//vr::IVRVirtualDisplay
	void Present(const vr::PresentInfo_t* pPresentInfo, uint32_t unPresentInfoSize) override;
	void WaitForPresent() override;
	bool GetTimeSinceLastVsync(float* pfSecondsSinceLastVsync, uint64_t* pulFrameCounter) override;

private:

	std::unique_ptr<CameraComponent> m_cameraComponent;
	std::unique_ptr<DisplayWindow> m_window;
	std::shared_ptr<D3D11Renderer> m_renderer;

	std::thread m_poseThread;
	std::atomic<bool> m_bRunThread = true;
	std::atomic<int> m_frameNumber = 0;

	vr::TrackedDeviceIndex_t m_deviceId = -1;

	std::array<vr::VRInputComponentHandle_t, 2> m_inputHandles{};

	bool m_bWaitForVSync = false;
	bool m_bHasVSyncTime = false;
	LARGE_INTEGER m_lastVsync;
	uint64_t m_frameCount = 0;

	int32_t m_windowPosX = 0;
	int32_t m_windowPosY = 0;

	int32_t m_windowWidth = 0;
	int32_t m_windowHeight = 0;

	int32_t m_renderWidth = 0;
	int32_t m_renderHeight = 0;
};
