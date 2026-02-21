#pragma once
#include "bnUserWindow.h"
#include "nGraphics/GraphicsUtilities.h"

class devBnLogoWindow
{
public:
	devBnLogoWindow();
private:
	struct UBOData
	{
		alignas(16) float uFillColor[4];  
		alignas(16) float uStrokeColor[4];   
		alignas(16) float uTime;             
		float _pad0[3];                      
		alignas(16) float uSpotDir[2];     
		float _padSpotDir[2];              
		alignas(16) float uSpotRadius;      
		float uSpotSoftness;                
		float _pad1[2];                     
	};
	void static staticUpdateFunction(bnUserWindow* window, void* userObject, unsigned int msg, uintptr_t wParam, intptr_t lParam);
	void updateFunction(bnUserWindow* window, unsigned int msg, uintptr_t wParam, intptr_t lParam);
private:
	ResourceHandle<bnUserWindow> windowHandle;
	ResourceHandle<IShader> fragmentShader;
	ResourceHandle<IShader> vertexShader;
	ResourceHandle<IDescriptorPool> dPool;
	ResourceHandle<IDescriptorSetLayout> dSetLayout;
	ResourceHandle<IInputLayout> inputLayout;
	ResourceHandle<IPipeline> pipeline;
	ResourceHandle<IDescriptorSet> set;
	ResourceHandle<IBuffer> vertexBuffer;
	ResourceHandle<IBuffer> uboBuffer;
	ResourceHandle<void> mappedData;
	UBOData uboBufferData;
	ResourceHandle<ITexture> texture;
	ShaderData vD;
	ShaderData pD;
	ResourceHandle<void> dataGPU;
	ResourceHandle<IBuffer> stagingBuffer;
	ResourceHandle<ISamplerState> samplerState;
};

