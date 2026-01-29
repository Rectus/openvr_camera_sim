
#include "pch.h"
#include "device_provider.h"

vr::EVRInitError DeviceProvider::Init(vr::IVRDriverContext* pDriverContext) 
{
    VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);
    vr::VRDriverLog()->Log("DeviceProvider::Init");

    m_cameraDevice = std::make_unique<CameraDevice>();

    bool ret = vr::VRServerDriverHost()->TrackedDeviceAdded("openvr_camera_sim_virtual_display_001", vr::TrackedDeviceClass_HMD, m_cameraDevice.get());

    if (ret)
    {
        vr::VRDriverLog()->Log("TrackedDeviceAdded() succeeded!");
    }

    return vr::VRInitError_None;
}

void DeviceProvider::Cleanup() 
{
    VR_CLEANUP_SERVER_DRIVER_CONTEXT();
}

const char* const* DeviceProvider::GetInterfaceVersions() 
{
    return vr::k_InterfaceVersions;
}

void DeviceProvider::RunFrame() 
{
    vr::VREvent_t vrevent;
    while (vr::VRServerDriverHost()->PollNextEvent(&vrevent, sizeof(vrevent))) 
    {
        m_cameraDevice->HandleEvent(vrevent);
    }

    if (m_cameraDevice != nullptr) 
    {
        m_cameraDevice->RunFrame();
    }
}

bool DeviceProvider::ShouldBlockStandbyMode() 
{
    return false;
}

void DeviceProvider::EnterStandby() 
{

}

void DeviceProvider::LeaveStandby() 
{

}