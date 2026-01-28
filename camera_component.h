#pragma once


struct CameraFrameContainer
{
	vr::CameraVideoStreamFrame_t FrameData = {};
	//std::vector<uint8_t> Framebuffer = {};
	uint8_t * Framebuffer = nullptr;
	std::shared_mutex AccessMutex;

	CameraFrameContainer()
	{

	}
};

class CameraComponent : public vr::IVRCameraComponent
{
public:

	CameraComponent();
	~CameraComponent();

	bool Init(vr::TrackedDeviceIndex_t HMDDeviceId);
	void Deinit();

	std::string& GetCameraDisplayName()
	{
		return m_cameraName;
	}

	// Inherited from IVRCameraComponent
	virtual bool GetCameraFrameDimensions(vr::ECameraVideoStreamFormat nVideoStreamFormat, uint32_t* pWidth, uint32_t* pHeight) override;
	virtual bool GetCameraFrameBufferingRequirements(int* pDefaultFrameQueueSize, uint32_t* pFrameBufferDataSize) override;
	virtual bool SetCameraFrameBuffering(int nFrameBufferCount, void** ppFrameBuffers, uint32_t nFrameBufferDataSize) override;
	virtual bool SetCameraVideoStreamFormat(vr::ECameraVideoStreamFormat nVideoStreamFormat) override;
	virtual vr::ECameraVideoStreamFormat GetCameraVideoStreamFormat() override;
	virtual bool StartVideoStream() override;
	virtual void StopVideoStream() override;
	virtual bool IsVideoStreamActive(bool* pbPaused, float* pflElapsedTime) override;
	virtual const vr::CameraVideoStreamFrame_t* GetVideoStreamFrame() override;
	virtual void ReleaseVideoStreamFrame(const vr::CameraVideoStreamFrame_t* pFrameImage) override;
	virtual bool SetAutoExposure(bool bEnable) override;
	virtual bool PauseVideoStream() override;
	virtual bool ResumeVideoStream() override;
	virtual bool GetCameraDistortion(uint32_t nCameraIndex, float flInputU, float flInputV, float* pflOutputU, float* pflOutputV) override;
	virtual bool GetCameraProjection(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, float flZNear, float flZFar, vr::HmdMatrix44_t* pProjection) override;
	virtual bool SetFrameRate(int nISPFrameRate, int nSensorFrameRate) override;
	virtual bool SetCameraVideoSinkCallback(vr::ICameraVideoSinkCallback* pCameraVideoSinkCallback) override;
	virtual bool GetCameraCompatibilityMode(vr::ECameraCompatibilityMode* pCameraCompatibilityMode) override;
	virtual bool SetCameraCompatibilityMode(vr::ECameraCompatibilityMode nCameraCompatibilityMode) override;
	virtual bool GetCameraFrameBounds(vr::EVRTrackedCameraFrameType eFrameType, uint32_t* pLeft, uint32_t* pTop, uint32_t* pWidth, uint32_t* pHeight) override;
	virtual bool GetCameraIntrinsics(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, vr::HmdVector2_t* pFocalLength, vr::HmdVector2_t* pCenter, vr::EVRDistortionFunctionType* peDistortionType, double rCoefficients[vr::k_unMaxDistortionFunctionParameters]) override;

protected:
	void ServeFrames();

	bool m_bIsInitialized = false;
	bool m_bIsStreamActive = false;
	bool m_bIsStreamPaused = false;

	vr::TrackedDeviceIndex_t m_HMDDeviceId = -1;
	int m_deviceNum = 0;
	int m_selectedMediaType = -1;
	vr::ECameraVideoStreamFormat m_streamFormat;
	std::string m_cameraName;

	uint32_t m_renderTargetWidth = 0;
	uint32_t m_renderTargetHeight = 0;
	uint32_t m_renderTargetBPP = 0;

	uint32_t m_frameWidth = 0;
	uint32_t m_frameHeight = 0;

	float m_focalX = 0;
	float m_focalY = 0;
	float m_centerX = 0;
	float m_centerY = 0;

	uint64_t m_frameSequence = 0;
	double m_frameTimeMonotonic = 0.0;
	uint64_t m_serverFrameTicks = 0;
	double m_deliveryRate = 0.0;
	double m_elapsedTime = 0.0;

	uint64_t m_frameCount = 0;

	LARGE_INTEGER m_startTime;
	LARGE_INTEGER m_lastFrameTime;
	LARGE_INTEGER m_firstStartTime;
	bool m_bIsFirstStart = true;

	vr::PropertyContainerHandle_t m_rawFrameQueue = 0;

	std::thread m_frameServeThread;
	std::atomic<bool> m_bRunThread = true;

	std::vector<uint8_t*> m_framebuffers;
	std::shared_ptr<CameraFrameContainer> m_currentFrame;
	std::shared_ptr<CameraFrameContainer> m_nextFrame;

	std::mutex m_frameContainerMutex;

	vr::ICameraVideoSinkCallback* m_pCameraVideoSinkCallback = nullptr;
};