#include "pch_bcl.h"
#include "SkyDome.h"
#include <Models.h>
#include <PrimitiveVisualizer.h>

using namespace Causality;
using namespace DirectX;

namespace Causality
{
	extern bool		g_ShowCharacterMesh;
}

SkyDome::SkyDome()
{
}

SkyDome::~SkyDome()
{

}

void SkyDome::CreateDeviceResource(ID3D11Device * device, DirectX::EnvironmentMapEffect * pEffect)
{
	m_pSphere = DirectX::Scene::GeometricPrimtives::CreateSphere(device, 1.0f, 16, true, true);
	m_pSphere->CreateInputLayout(device, pEffect);
	m_pEffect = pEffect;
}


void SkyDome::SetTexture(DirectX::Texture & texture)
{
	m_Texture = texture;
}

// Inherited via IVisual

bool SkyDome::IsVisible(const DirectX::BoundingGeometry & viewFrustum) const
{
	return g_ShowCharacterMesh;
}

void SkyDome::Render(IRenderContext * context, DirectX::IEffect * pEffect)
{
	auto pStates = Visualizers::g_PrimitiveDrawer.GetStates();
	m_pEffect->SetTexture(NULL);
	m_pEffect->SetWorld(XMMatrixIdentity());
	m_pEffect->SetEnvironmentMap(m_Texture.ShaderResourceView());
	m_pEffect->SetEnvironmentMapAmount(0.5f);
	m_pEffect->SetDiffuseColor(DirectX::Colors::White.v);
	m_pEffect->SetAmbientLightColor(DirectX::Colors::Azure.v);
	m_pEffect->SetFresnelFactor(0.0f);
	m_pEffect->Apply(context);

	ComPtr<ID3D11DepthStencilState> pFomerState;
	UINT sRef;
	context->RSSetState(pStates->CullClockwise());
	context->OMGetDepthStencilState(&pFomerState, &sRef);
	context->OMSetDepthStencilState(pStates->DepthNone(), sRef);

	m_pSphere->Draw(context, m_pEffect);

	context->OMSetDepthStencilState(pFomerState.Get(), sRef);
	context->RSSetState(pStates->CullCounterClockwise());
}

void XM_CALLCONV SkyDome::UpdateViewMatrix(DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection)
{
	XMMATRIX View = view;
	// Last column of View Inverse is camera's position
	View.r[3] = g_XMIdentityR3;
	m_pEffect->SetView(View);
	m_pEffect->SetProjection(projection);
}

// Inherited via IVisual

RenderFlags SkyDome::GetRenderFlags() const
{
	return RenderFlags::SkyView;
}