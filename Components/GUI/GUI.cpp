#include "GUI.h"

#include "Globals/Globals.h"

#include "RiotDebug.h"
#include "Components/TUI/TUI.h"

#include <chrono>
#include <d3d11.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>
#include <imgui.h>
#include <thread>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND, UINT, WPARAM, LPARAM );

namespace Components
{
	namespace
	{
		ID3D11Device*           pDevice           = nullptr;
		ID3D11DeviceContext*    pDeviceContext    = nullptr;
		IDXGISwapChain*         pSwapChain        = nullptr;
		ID3D11RenderTargetView* pMainRTV          = nullptr;
		ID3D11Buffer*           pBackBuffer       = nullptr;
		HWND                    hWindow           = nullptr;
		constexpr UINT          CreateDeviceFlags = 0;
		D3D_FEATURE_LEVEL       FeatureLevel;

		LRESULT WINAPI WndProc( const HWND hWnd, const UINT Message, const WPARAM wParam, const LPARAM lParam )
		{
			if ( ImGui_ImplWin32_WndProcHandler( hWnd, Message, wParam, lParam ) ) return true;

			return ::DefWindowProcW( hWnd, Message, wParam, lParam );
		}

		void InitializeWindow()
		{
			const auto nScreenWidth  = static_cast<float>( GetSystemMetrics( SM_CXSCREEN ) );
			const auto nScreenHeight = static_cast<float>( GetSystemMetrics( SM_CYSCREEN ) );

			const wchar_t* WindowName = L"RobotBuddy Debug";

			const auto NormalCursor = LoadCursorW( nullptr, IDC_ARROW );

			const WNDCLASSEX ClassEx = { sizeof( WNDCLASSEX ), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle( nullptr ), nullptr, NormalCursor, nullptr, nullptr, WindowName, nullptr };

			RegisterClassEx( &ClassEx );

			hWindow = CreateWindowExW(
			                          0,
			                          ClassEx.lpszClassName,
			                          WindowName,
			                          0,
			                          100, 100,
			                          1500,
			                          1000,
			                          nullptr, nullptr,
			                          ClassEx.hInstance,
			                          nullptr
			                         );

			ShowWindow( hWindow, SW_SHOWDEFAULT );
			UpdateWindow( hWindow );

			DXGI_SWAP_CHAIN_DESC sc;
			ZeroMemory( &sc, sizeof sc );

			sc.BufferCount                        = 2;
			sc.BufferDesc.Width                   = 0;
			sc.BufferDesc.Height                  = 0;
			sc.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
			sc.BufferDesc.RefreshRate.Numerator   = 30;
			sc.BufferDesc.RefreshRate.Denominator = 1;
			sc.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			sc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sc.OutputWindow                       = hWindow;
			sc.SampleDesc.Count                   = 1;
			sc.SampleDesc.Quality                 = 0;
			sc.Windowed                           = TRUE;
			sc.SwapEffect                         = DXGI_SWAP_EFFECT_SEQUENTIAL;

			HRESULT Result = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, CreateDeviceFlags, nullptr, 0, D3D11_SDK_VERSION, &sc, &pSwapChain, &pDevice, &FeatureLevel, &pDeviceContext );

			if ( !SUCCEEDED( Result ) )
			{
				PrintError( "CreateDeviceAndSwapChain Failure: 0x{:X}", Result );
				return;
			}

			if ( Result = pSwapChain->GetBuffer( 0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>( &pBackBuffer ) ); Result != S_OK )
			{
				PrintError( "pSwapChain->GetBuffer Failure: 0x{:X}", Result );
				return;
			}

			if ( Result = pDevice->CreateRenderTargetView( pBackBuffer, nullptr, &pMainRTV ); Result != S_OK )
			{
				PrintError( "pDevice->CreateRenderTargetView Failure: 0x{:X}", Result );
				return;
			}

			pBackBuffer->Release();
		}
	}


	void GUI::Initialize()
	{
		InitializeWindow();
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		ImGui_ImplWin32_Init( hWindow );
		ImGui_ImplDX11_Init( pDevice, pDeviceContext );
		ImGui::StyleColorsDark();
	}

	void GUI::Execute()
	{
		MSG Message;
		ZeroMemory( &Message, sizeof(Message) );

		float ClearColor[ 4 ] = { 0.f, 0.f, 0.f, 0.f };
		RECT  WindowRect;

		using namespace std::chrono;
		constexpr auto FrameDuration = milliseconds( 17 ); // ~60fps (maybe export this to config?)

		constexpr auto Flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

		while ( Message.message != WM_QUIT )
		{
			auto FrameStart = high_resolution_clock::now();

			while ( PeekMessage( &Message, nullptr, 0U, 0U, PM_REMOVE ) )
			{
				TranslateMessage( &Message );
				DispatchMessage( &Message );
			}

			if ( Message.message == WM_QUIT ) break;

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			auto& IO = ImGui::GetIO();

			ImGui::SetNextWindowPos( { 0, 0 }, ImGuiCond_Always );
			ImGui::SetNextWindowSize( IO.DisplaySize, ImGuiCond_Always );

			ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0, 0 ) );
			ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.f );
			ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0, 0, 0, 0 ) );

			Debug::Draw();

			ImGui::PopStyleColor();
			ImGui::PopStyleVar( 2 );

			pDeviceContext->OMSetRenderTargets( 1, &pMainRTV, nullptr );
			pDeviceContext->ClearRenderTargetView( pMainRTV, reinterpret_cast<FLOAT*>( &ClearColor ) );
			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );
			( void )pSwapChain->Present( 1, 0 );

			auto FrameEnd = high_resolution_clock::now();
			auto Elapsed  = duration_cast<milliseconds>( FrameEnd - FrameStart );

			if ( Elapsed < FrameDuration ) std::this_thread::sleep_for( FrameDuration - Elapsed );
		}
	}
}
