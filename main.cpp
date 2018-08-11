#include "Externals/SDL/Include/SDL.h"
#include "Externals/SDL/Include/SDL_syswm.h"

// Own Engine Headers
#include "CustomExceptions/Direct3dException.h"

// We include atlbase in order to use the ATL smart pointer
// Microsoft::WRL:ComPtr (template smart-pointer for COM objects)
#include <wrl/client.h>
using namespace Microsoft::WRL;

// Direct3D related header files
#include <d3d11.h>
#include <d3dcompiler.h>
#include <d3dcommon.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
using namespace DirectX;

/*
 * For SDL it is important that SDL2main.lib is linked BEFORE
 * SDL2.lib. This is because SDL2main.lib defines the actual entry point
 * For this application.
 * 
 * I have needed to use the pragma comment approach of linking the two libraries
 * For now in order to force the actual linking order. Using the properties window
 * of the project properties doesn't seem to adhere to a linking order determined
 * by the list you write (from top to bottom)
 */
#pragma comment(lib, "Externals/SDL/SDL2main.lib")
#pragma comment(lib, "Externals/SDL/SDL2.lib")

// Include Direct3D libraries
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

// Direct3D variables
ComPtr<IDXGISwapChain> direct3dSwapChain;
ComPtr<ID3D11Device> direct3dDevice;
ComPtr<ID3D11DeviceContext> direct3dDeviceContext;
ComPtr<ID3D11RenderTargetView> mRenderTargetView;
ComPtr<ID3D11DepthStencilView> mDepthStencilView;
D3D_FEATURE_LEVEL direct3dFeatureLevel;

typedef struct VertexDefinition1
{
	XMFLOAT3 Position;
} VertexWithPosition;

// Function Prototypes
void InitializeDirect3d(HWND windowHandle);

int main(int argc, char *argv[])
{
	// SDL Init must be called before any other SDL function
	// This is in order to initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) != 0)
	{
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
		return 1;
	}

	SDL_Log("SDL initialized...");
	
	SDL_Log("Initializing main window...");

	auto mainWindow = SDL_CreateWindow(
		"Rotating Cube",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		640,
		480,
		0
	);

	if (mainWindow == nullptr)
	{
		SDL_Log("Unable to create main application window: %s", SDL_GetError());
		return 1;
	}

	SDL_Log("Main application window created...");

	// We need to get the window handle for Direct3D initialization
	// This is a structure that contains system-dependent information about a window
	// We fill this structure using SDL_GetWindowWMInfo()
	// However, before we call SDL_GetWindowWMInfo, we must initialize
	// The SDL_SysWMinfo.version property using SDL_VERSION
	SDL_SysWMinfo systemInformation;
	SDL_VERSION(&systemInformation.version);
	SDL_GetWindowWMInfo(mainWindow, &systemInformation);

	auto windowHandle = systemInformation.info.win.window;

	try
	{
		SDL_Log("Initializing Direct3D...");
		InitializeDirect3d(windowHandle);
	}
	catch (Direct3dException ex)
	{
		SDL_Log("An error occured initialization Direct3D: %s", ex.what());
		return 1;
	}

	bool quit = false;

	while (!quit)
	{
		SDL_Event e;
		if (SDL_PollEvent(&e) != 0)
		{
			if (e.type == SDL_QUIT)
				quit = true;
		}

		// Clear the back buffer to deep blue
		direct3dDeviceContext->ClearRenderTargetView(mRenderTargetView.Get(), DirectX::Colors::CornflowerBlue);

		direct3dDeviceContext->ClearDepthStencilView(
			mDepthStencilView.Get(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,
			0
		);

		// Render stuff here!

		// Switch the back buffer and the front buffer
		direct3dSwapChain->Present(0, 0);
	}

	// SDL Quit should be called before an SDL application exits, to safely shut down
	// All subsystems.
	SDL_Quit();

	return 0;
}

void InitializeDeviceAndDeviceContext()
{
	SDL_Log("Initializing Direct3D Device and DeviceContext...");

	// The first part of Direct3D initialization consists of creating the DIrect3D 11 device and context.
	// The device is used to check feature support and also to allocate resources
	// The device context interface is used to set render states, bind resources to the graphics pipeline, and to issue
	// rendering commands.
	HRESULT deviceCreationResult = D3D11CreateDevice(
		// We specify null for display adapter (makes it use the primary display adapter)
		nullptr,
		// D3D_DRIVER_TYPE_HARDWARE specifies that we use hardware acceleration for rendering
		D3D_DRIVER_TYPE_HARDWARE,
		// Used for specifying a software driver. We use hardware rendering, so we set this to NULL.
		nullptr,
		// Optional device creation flags
		// D3D11_CREATE_DEVICE_DEBUG = Enables the debug layer
		// D3D11_CREATE_DEVICE_SINGLETHREADED = Improves performance if you can guarentee that Direct3D will not be called
		// By multiple threads.
		D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_SINGLETHREADED,
		// An array of D3D_FEATURE_LEVEL elements, which is used to test supported features on the
		// Running hardware
		0, 0,
		// The SDK Version. Simply always specify D3D11_SDL_VERSION here
		D3D11_SDK_VERSION,
		direct3dDevice.GetAddressOf(),
		// Feature levels supported. Not using this right now, probably should at some point ;)
		nullptr,
		direct3dDeviceContext.GetAddressOf()
	);

	if (deviceCreationResult != S_OK)
	{
		const auto errorMessage = "Error initializing device or device context. Error code: " + std::to_string(
			deviceCreationResult);

		throw Direct3dException(errorMessage);
	}
}

void InitializeSwapChain(HWND windowHandle)
{
	SDL_Log("Initializing Direct3D swapchain...");

	// Next we need to create the swap chain
	// In order to create the swap chain, we need to fill out an instance of the DXGI_SWAP_CHAIN_DESC structure, which is used to describe
	// the characteristics of the swap chain we want to create.
	// The Swap Chain represents the front and back buffer used for rendering in a way that attempts to remove flickering.
	// The swap chain consists of a front and back buffer. Entire frames are drawn to the back buffer, at which point
	// The back buffer and front buffer are switched in order to present the frame on the monitor.
	// Switching these two buffers is called "presenting".
	DXGI_SWAP_CHAIN_DESC sd;

	// The BufferDesc struct describes the backbuffer
	sd.BufferDesc.Width = 0; // Putting 0 for width will let the runtime determine it automatically by the output window
	sd.BufferDesc.Height = 0; // Putting 0 for the height will let the runtime determine it automatically by the output window
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	// SampleDesc describes multi-sampling parameters
	// Count = 1 and Quality = 0 means no anti-aliasing
	sd.SampleDesc.Count = 1; // Number of multi-samples per pixel
	sd.SampleDesc.Quality = 0; // The image quality level. Higher quality = lower performance

							   // We specify that we use the given surface / resource as output render target
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	// The amount of buffers in the swap chain.
	sd.BufferCount = 1;

	// A handle to the output window
	sd.OutputWindow = windowHandle;

	// Specifies if the output is in windowed mode.
	sd.Windowed = true;

	// This flag should be used to enable the display driver to select the most efficient
	// Presentation technique for the swap chain
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = 0;

	// In order to actually create the swap chain, we need to go through the IDXGIFactory interface.
	// However, we have to get the IDXGIFactory instance that was also used to create the device.
	ComPtr<IDXGIDevice> dxgiDevice;
	const auto dxgiDeviceCastResult = direct3dDevice.As(&dxgiDevice);

	if (dxgiDeviceCastResult != S_OK)
		throw Direct3dException("Failed to retrive interface for IDXGIDevice. Error code: " 
			+ std::to_string(dxgiDeviceCastResult));

	ComPtr<IDXGIAdapter> dxgiAdapter;
	const auto idxgiAdapterRetrievalResult = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(dxgiAdapter.GetAddressOf()));

	if (idxgiAdapterRetrievalResult != S_OK)
		throw Direct3dException("Failed to retrieve parent IDXGIAdapter from IDXGIDevice. Error code: "
			+ std::to_string(idxgiAdapterRetrievalResult));

	// Finally get the IDXGIFactory interface
	ComPtr<IDXGIFactory> dxgiFactory;
	const auto idxgiFactoryRetrievalResult = dxgiAdapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(dxgiFactory.GetAddressOf()));

	if (idxgiFactoryRetrievalResult != S_OK)
		throw Direct3dException("Failed to retrieve parent IDXGIFactory from IDXGIFactory. Error code: " 
			+ std::to_string(idxgiFactoryRetrievalResult));

	// Now we can finally create the swap chain
	const auto swapChainCreationResult = dxgiFactory->CreateSwapChain(direct3dDevice.Get(), &sd, direct3dSwapChain.GetAddressOf());

	if (swapChainCreationResult != S_OK)
		throw Direct3dException("Failed to create swapchain. Error code: " 
			+ std::to_string(swapChainCreationResult));
}

void InitializeBackBufferAndDepthStencilView()
{
	// Now we must create the render target view for our backbuffer
	// The special thing about a render target view is that it can
	// Be bound to the output-merger stage by calling
	// ID3D11DeviceContext::OMSetRenderTargets
	ComPtr<ID3D11Texture2D> backBuffer;

	const auto getSwapChainBufferResult = direct3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
		reinterpret_cast<void**>(backBuffer.GetAddressOf()));

	if (getSwapChainBufferResult != S_OK)
		throw Direct3dException("Failed to retrieve swapchain back buffer. Error Code"
			+ std::to_string(getSwapChainBufferResult));

	const auto createRenderTargetViewResult =
		direct3dDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, mRenderTargetView.GetAddressOf());

	if (createRenderTargetViewResult != S_OK)
		throw Direct3dException("Failed to create render target view. Error Code: " 
			+ std::to_string(createRenderTargetViewResult));

	// Now we need to create the depth/stencil buffer
	// This is just a 2D texture that stores the depth information
	D3D11_TEXTURE2D_DESC depthStencilDesc;
	depthStencilDesc.Width = 640;
	depthStencilDesc.Height = 480;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	// We do not want to use anti-aliasing right now
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;

	ComPtr<ID3D11Texture2D> mDepthStencilBuffer;

	const auto texture2dCreationResult =
		direct3dDevice->CreateTexture2D(&depthStencilDesc, 0, mDepthStencilBuffer.GetAddressOf());

	if (texture2dCreationResult != S_OK)
		throw Direct3dException("Failed to create 2D texture for depth stencil buffer. Error Code: " + texture2dCreationResult);

	const auto depthStencilViewCreationResult = 
		direct3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), 0, mDepthStencilView.GetAddressOf()); 

	if (depthStencilViewCreationResult != S_OK)
		throw Direct3dException("Failed to create depth stencil view. Error Code: "
			+ std::to_string(depthStencilViewCreationResult));

	// Bind views to the output merger stage
	// The output-merger stage generates the final
	// rendered pixel color.
	// This stage is the final step for determining which pixels are visible
	// (with depth-stencil testing) and blending the final pixel colors.
	// We give out render target view for the backbuffer here, so that the final
	// Images can be rendered to it and presented to the screen through the swapchain
	direct3dDeviceContext->OMSetRenderTargets(
		1,
		mRenderTargetView.GetAddressOf(),
		mDepthStencilView.Get()
	);
}

void InitializeViewport()
{
	// We create the viewport
	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width = static_cast<float>(800);
	vp.Height = static_cast<float>(600);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	direct3dDeviceContext->RSSetViewports(1, &vp);
}

void InitializeDirect3d(HWND windowHandle)
{
	InitializeDeviceAndDeviceContext();
	InitializeSwapChain(windowHandle);
	InitializeBackBufferAndDepthStencilView();
	InitializeViewport();
}