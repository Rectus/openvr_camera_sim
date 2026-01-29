OpenVR Camera Simulation Driver
---

This SteamVR driver showcases a basic implementation of a simulated HMD with working passthrough cameras. It creates a simulated HMD using the `IVRDisplayComponent` , `IVRVirtualDisplay`, and `IVRCameraComponent` classes, displaying the headset output in a custom desktop window. 

The passthrough camera systems in SteamVR seem to be hardcoded for the lighthouse driver, and only using the `IVRCameraComponent` is not sufficient for SteamVR to process the passthrough. The camera framebuffers and per-frame data are instead submitted using the `IVRBlockQueue` and `IVRPaths` classes, only publicly available in the OpenVR C (not C++) headers.

The camera output works with both the 2D Room View mode, and applications using the `IVRTrackedCamera` interface. The stereo correct 3D Room View mode is not currently supported.

Note: I haven't verified if all the projection parameters are correct with a real camera.

The main files of interest are:
camera_component.cpp - Camera implementation
camera_device.cpp - HMD implementation
vr_blockqueue.h - C++ header versions of `IVRBlockQueue` and `IVRPaths`


The repo also contains `camera_buffer_snooper`, a client utility that prints out any frame metadata sent to the block queue.