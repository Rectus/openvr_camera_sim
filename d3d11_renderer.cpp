
#include "pch.h"
#include "d3d11_renderer.h"


static char copyTextureVS[] = R""""(

struct VS_OUTPUT
{
	float4 position: SV_Position;
	float2 UVs : TEXCOORD0;
};

VS_OUTPUT main(uint vertexID : SV_VertexID)
{
	VS_OUTPUT output;

	// Single triangle from (0, 0) to (2, 2).
	float2 clipCoords = float2((min(vertexID, 2) >> 1) * 2, (min(vertexID, 2) & 1) * 2) * 2 - 1;

	output.position = float4(clipCoords, 0, 1);
	output.UVs = clipCoords * float2(0.5, -0.5) + 0.5;

	return output;
}
)"""";

static char copyTexturePS[] = R""""(

struct VS_OUTPUT
{
	float4 position: SV_Position;
	float2 UVs : TEXCOORD0;
};

SamplerState g_samplerState : register(s0);
Texture2D<float4> g_inputTexture : register(t0);

float3 sRGBtoLinear(float3 srgb)
{
	float3 lin;
	lin.r = (srgb.r > 0.04045) ? pow((srgb.r + 0.055) / 1.055, 2.4) : srgb.r / 12.92;
    lin.g = (srgb.g > 0.04045) ? pow((srgb.g + 0.055) / 1.055, 2.4) : srgb.g / 12.92;
    lin.b = (srgb.b > 0.04045) ? pow((srgb.b + 0.055) / 1.055, 2.4) : srgb.b / 12.92;
	return lin;
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
	float4 sample = g_inputTexture.Sample(g_samplerState, input.UVs);

	// The compositor outputs a shared texture with a linear format specified, despite being sRGB encoded. 
	return float4(sRGBtoLinear(sample.rgb), sample.a);

	//return float4(0.5, 0.5, 0.5, 1);
	//return float4(input.UVs, 0.0, 1);
}
)"""";


D3D11Renderer::D3D11Renderer(HWND window)
	: m_windowHandle (window)
{

}

D3D11Renderer::~D3D11Renderer()
{

}

bool D3D11Renderer::InitRenderer()
{
	
	HRESULT res;

	IDXGIFactory1* factory1 = nullptr;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&factory1))))
	{
		vr::VRDriverLog()->Log("CreateDXGIFactory failure!");
		return false;
	}

	IDXGIFactory6* factory6 = nullptr;
	if (FAILED(factory1->QueryInterface(__uuidof(IDXGIFactory6), (void**)(&factory6))))
	{
		vr::VRDriverLog()->Log("Querying IDXGIFactory6 failure!");
		factory1->Release();
		return false;
	}
	factory1->Release();

	IDXGIAdapter1* adapter = nullptr;

	res = FAILED(factory6->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter1), (void**)&adapter));
	if(FAILED(res))
	{
		std::string message = std::format("EnumAdapterByGpuPreference failed: 0x{:X}", res);
		vr::VRDriverLog()->Log(message.c_str());
		factory6->Release();
		return false;
	}

	factory6->Release();

	DXGI_ADAPTER_DESC adapterDesc{};
	if (FAILED(adapter->GetDesc(&adapterDesc)))
	{
		vr::VRDriverLog()->Log("GetDesc failure!");
		factory6->Release();
		adapter->Release();
		return false;
	}

	m_deviceLUID.structLuid = adapterDesc.AdapterLuid;

	{
		std::wstring wdesc = std::wstring(adapterDesc.Description);
		std::string desc = std::string(wdesc.begin(), wdesc.end());
		std::string message = std::format("Using graphics adapter {} with LUID {}", desc, m_deviceLUID.uintLUID);
		vr::VRDriverLog()->Log(message.c_str());
	}

	std::vector<D3D_FEATURE_LEVEL> featureLevels = { D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1 };

	DXGI_SWAP_CHAIN_DESC swapchainDesc = {};
	swapchainDesc.BufferCount = 2;
	swapchainDesc.BufferDesc.Width = 0;
	swapchainDesc.BufferDesc.Height = 0;
	swapchainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapchainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchainDesc.OutputWindow = m_windowHandle;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.Windowed = TRUE;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	res = D3D11CreateDeviceAndSwapChain(adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, featureLevels.data(), (uint32_t)featureLevels.size(), D3D11_SDK_VERSION, &swapchainDesc, &m_swapChain, &m_device, NULL, &m_deviceContext);

	adapter->Release();

	if (FAILED(res))
	{
		std::string message = std::format("D3D11CreateDeviceAndSwapChain failure: 0x{:X}", (unsigned long)res);
		vr::VRDriverLog()->Log(message.c_str());
		return false;
	}

	ComPtr<ID3DBlob> vsBlob;
	ComPtr<ID3DBlob> errorBlob;

	res = D3DCompile(copyTextureVS, sizeof(copyTextureVS), nullptr, nullptr, nullptr, "main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &vsBlob, &errorBlob);

	if (FAILED(res))
	{
		VR_DRIVER_LOG_FORMAT("D3DCompile copyTextureVS failure: 0x{:X}", (unsigned long)res);
		VR_DRIVER_LOG_FORMAT("{}", (char*)errorBlob->GetBufferPointer());
		return false;
	}

	if (FAILED(m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_copyTextureVS)))
	{
		vr::VRDriverLog()->Log("m_copyTextureVS creation failure!");
		return false;
	}

	ComPtr<ID3DBlob> psBlob;

	res = D3DCompile(copyTexturePS, sizeof(copyTexturePS), nullptr, nullptr, nullptr, "main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, &psBlob, &errorBlob);

	if (FAILED(res))
	{
		VR_DRIVER_LOG_FORMAT("D3DCompile copyTexturePS failure: 0x{:X}", (unsigned long)res);
		VR_DRIVER_LOG_FORMAT("{}", (char*)errorBlob->GetBufferPointer());
		return false;
	}

	if (FAILED(m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_copyTexturePS)))
	{
		vr::VRDriverLog()->Log("m_copyTexturePS creation failure!");
		return false;
	}

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(m_device->CreateSamplerState(&samplerDesc, m_bilinearSampler.GetAddressOf())))
	{
		vr::VRDriverLog()->Log("m_bilinearSampler creation failure!\n");
		return false;
	}

	UpdateWindowSize();

	return true;
}

void D3D11Renderer::Render(const vr::PresentInfo_t* pPresentInfo, bool bWaitForVsync)
{
	m_bWaitForVSync = bWaitForVsync;

	if (FAILED(m_device->OpenSharedResource(reinterpret_cast<void*>(pPresentInfo->backbufferTextureHandle), IID_PPV_ARGS(m_frameTexture.GetAddressOf()))))
	{
		std::string message = std::format("OpenSharedResource failed: {:X}", (unsigned long)GetLastError());
		vr::VRDriverLog()->Log(message.c_str());
		return;
	}

	ComPtr<IDXGIKeyedMutex> frameMutex;

	if(FAILED(m_frameTexture->QueryInterface(IID_PPV_ARGS(frameMutex.GetAddressOf()))))
	{
		vr::VRDriverLog()->Log("QueryInterface for mutex failed!");
		return;
	}

	if (FAILED(frameMutex->AcquireSync(0, 10)))
	{
		frameMutex->Release();
		vr::VRDriverLog()->Log("AcquireSync failed!");
		return;
	}

	D3D11_TEXTURE2D_DESC frameDesc = {};
	m_frameTexture->GetDesc(&frameDesc);

	//VR_DRIVER_LOG_FORMAT("Compositor output: {} x {} x {}, format: {}", frameDesc.Width, frameDesc.Height, frameDesc.ArraySize, (int)frameDesc.Format)

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = frameDesc.Format;
	srvDesc.Texture2D.MipLevels = 1;

	if (FAILED(m_device->CreateShaderResourceView(m_frameTexture.Get(), &srvDesc, &m_frameSRV)))
	{
		vr::VRDriverLog()->Log("m_frameSRV creation error!");
		return;
	}

	if (!IsWindowVisible(m_windowHandle))
	{
		ShowWindow(m_windowHandle, SW_SHOW);
	}

	UpdateWindowSize();

	ComPtr<ID3D11Texture2D> backBuffer;
	m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));

	D3D11_TEXTURE2D_DESC backBufferDesc = {};
	backBuffer->GetDesc(&backBufferDesc);

	//VR_DRIVER_LOG_FORMAT("backBufferDesc: {} x {} x {}, format: {}", backBufferDesc.Width, backBufferDesc.Height, backBufferDesc.ArraySize, (int)backBufferDesc.Format)

	m_deviceContext->ClearState();

	const float clearColor[4] = { 0.5, 0, 0, 1 };
	m_deviceContext->ClearRenderTargetView(m_outputRTV.Get(), clearColor);

	D3D11_VIEWPORT viewport = { 0.0f, 0.0f, (float)m_windowWidth, (float)m_windowHeight, 0.0f, 1.0f };

	m_deviceContext->RSSetViewports(1, &viewport);
	m_deviceContext->RSSetScissorRects(0, nullptr);
	m_deviceContext->RSSetState(nullptr);

	m_deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	m_deviceContext->VSSetShader(m_copyTextureVS.Get(), nullptr, 0);
	m_deviceContext->PSSetShader(m_copyTexturePS.Get(), nullptr, 0);

	m_deviceContext->PSSetShaderResources(0, 1, m_frameSRV.GetAddressOf());

	ID3D11RenderTargetView* rtv = m_outputRTV.Get();
	m_deviceContext->OMSetRenderTargets(1, &rtv, nullptr);
	m_deviceContext->OMSetBlendState(nullptr, nullptr, UINT_MAX);

	m_deviceContext->Draw(3, 0);

	m_deviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	frameMutex->ReleaseSync(0);
	frameMutex->Release();
}

void D3D11Renderer::FinishRender()
{
	HRESULT hr = m_swapChain->Present(m_bWaitForVSync ? 1 : 0, 0);
}

void D3D11Renderer::UpdateWindowSize()
{
	RECT winRect;
	if (!GetWindowRect(m_windowHandle, &winRect))
	{
		std::string message = std::format("GetWindowRect failed: {:X}", (unsigned long)GetLastError());
		vr::VRDriverLog()->Log(message.c_str());
		return;
	}
	int width = winRect.right - winRect.left;
	int height = winRect.bottom - winRect.top;

	if (width != m_windowWidth || height != m_windowHeight)
	{

		if (m_outputRTV.Get())
		{
			m_outputRTV.Reset();
		}

		m_windowWidth = width;
		m_windowHeight = height;
		m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);


		ComPtr<ID3D11Texture2D> backBuffer;
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

		m_device->CreateRenderTargetView(backBuffer.Get(), &rtvDesc, &m_outputRTV);
	}
}
