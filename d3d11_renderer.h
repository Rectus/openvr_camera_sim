
#pragma once


union LUID_UNION
{
	LUID structLuid;
	uint64_t uintLUID;
};


class D3D11Renderer
{
public:

	D3D11Renderer(HWND window);
	~D3D11Renderer();

	bool InitRenderer();

	bool IsInitialized() const { return m_bIsIntialized; }
	LUID_UNION GetLUID() const { return m_deviceLUID; }

	void Render(const vr::PresentInfo_t* pPresentInfo, bool bWaitForVsync);
	void FinishRender();

protected:

	void UpdateWindowSize();

	bool m_bIsIntialized = false;

	HWND m_windowHandle = 0;

	LUID_UNION m_deviceLUID = { 0 };

	ComPtr<ID3D11Device> m_device;
	ComPtr<ID3D11DeviceContext> m_deviceContext;
	ComPtr<IDXGISwapChain> m_swapChain;

	ComPtr <ID3D11Texture2D> m_frameTexture;
	ComPtr <ID3D11ShaderResourceView> m_frameSRV;

	ComPtr<ID3D11RenderTargetView> m_outputRTV;

	ComPtr<ID3D11SamplerState> m_bilinearSampler;

	ComPtr<ID3D11VertexShader> m_copyTextureVS;
	ComPtr<ID3D11PixelShader> m_copyTexturePS;

	uint32_t m_windowWidth = 0;
	uint32_t m_windowHeight = 0;

	bool m_bWaitForVSync = false;
};

