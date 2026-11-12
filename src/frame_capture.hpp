#pragma once
#include <d3d11.h>
#include <filesystem>
#include <fstream>
#include <vector>

// Optional diagnostic: copy the actual D3D11 backbuffer, including the ImGui HUD.
// A BMP keeps capture independent of third-party image encoders and GPU timing.
inline bool capture_backbuffer(ID3D11Device* device,ID3D11DeviceContext* context,IDXGISwapChain* swapchain,const std::filesystem::path& file){
 ID3D11Texture2D* source=nullptr;ID3D11Texture2D* staging=nullptr;
 if(FAILED(swapchain->GetBuffer(0,IID_PPV_ARGS(&source))))return false;
 D3D11_TEXTURE2D_DESC desc{};source->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
 if(FAILED(device->CreateTexture2D(&desc,nullptr,&staging))){source->Release();return false;}
 context->CopyResource(staging,source);source->Release();D3D11_MAPPED_SUBRESOURCE mapped{};
 if(FAILED(context->Map(staging,0,D3D11_MAP_READ,0,&mapped))){staging->Release();return false;}
 std::vector<uint32_t> pixels(size_t(desc.Width)*desc.Height);
 for(UINT y=0;y<desc.Height;++y){auto row=reinterpret_cast<const uint32_t*>(static_cast<const unsigned char*>(mapped.pData)+size_t(y)*mapped.RowPitch);for(UINT x=0;x<desc.Width;++x){uint32_t color=row[x];pixels[size_t(y)*desc.Width+x]=(color&0xff00ff00u)|((color&255u)<<16)|((color>>16)&255u);}}
 context->Unmap(staging,0);staging->Release();
 BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);header.bfSize=header.bfOffBits+DWORD(pixels.size()*4);
 info.biSize=sizeof(info);info.biWidth=LONG(desc.Width);info.biHeight=-LONG(desc.Height);info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=DWORD(pixels.size()*4);
 std::ofstream out(file,std::ios::binary);if(!out)return false;out.write(reinterpret_cast<const char*>(&header),sizeof(header));out.write(reinterpret_cast<const char*>(&info),sizeof(info));out.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()*4));return out.good();
}
