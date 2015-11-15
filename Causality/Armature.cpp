#include "pch_bcl.h"
#include "Armature.h"
#include "Animations.h"
#include <boost\range.hpp>
#include <boost\range\adaptors.hpp>
#include <boost\range\algorithm.hpp>
#include <regex>
#include <boost\assign.hpp>
#include <iostream>
#include <Eigen\Eigen>

using namespace DirectX;
using namespace Causality;

//void Bone::UpdateGlobalData(const Bone & reference)
//{
//	XMVECTOR ParQ = reference.GblRotation;
//	XMVECTOR Q = XMQuaternionMultiply(ParQ, LclRotation);
//	GblRotation = Q;
//
//	OriginPosition = reference.GblTranslation;
//	//XMVECTOR V = LclScaling;
//	//V = XMVectorMultiply(V, g_XMIdentityR1.v);
//	//V = XMVector3Rotate(V, Q);
//
//	V = XMVectorAdd(V, OriginPosition);
//	GblTranslation = V;
//}

Bone::Bone()
	: LclScaling(1.0f), LclLength(1.0f), GblScaling(1.0f), GblLength(1.0f), LclTw(1.0f), GblTw(1.0f)
{
}

void Bone::UpdateGlobalData(const Bone & reference)
{
	XMVECTOR ParQ = XMLoadA(reference.GblRotation);
	XMVECTOR Q = XMQuaternionMultiply(XMLoadA(LclRotation), ParQ);
	XMVECTOR ParS = XMLoadA(reference.GblScaling);
	XMVECTOR S = ParS * XMLoadA(LclScaling);
	GblRotation.StoreA(Q);
	GblScaling.StoreA(S);

	//OriginPosition = reference.GblTranslation; // should be a constriant

	XMVECTOR V = XMLoadA(LclTranslation);

	LclLength = XMVectorGetX(XMVector3Length(V));

	V *= ParS;

	GblLength = XMVectorGetX(XMVector3Length(V));

	V = XMVector3Rotate(V, ParQ);//ParQ
	V = XMVectorAdd(V, XMLoadA(reference.GblTranslation));

	GblTranslation.StoreA(V);
}

// This will assuming LclTranslation is not changed
void Bone::UpdateLocalData(const Bone& reference)
{
	//OriginPosition = reference.GblTranslation;
	XMVECTOR InvParQ = reference.GblRotation;
	InvParQ = XMQuaternionInverse(InvParQ); // PqInv
	XMVECTOR Q = GblRotation;
	Q = XMQuaternionMultiply(Q, InvParQ);
	LclRotation.StoreA(Q);

	Q = (XMVECTOR)GblTranslation - (XMVECTOR)reference.GblTranslation;
	//Q = XMVector3Length(Q);
	//Q = XMVectorSelect(g_XMIdentityR1.v, Q, g_XMIdentityR1.v);

	GblLength = XMVectorGetX(XMVector3Length(Q));

	Q = XMVector3Rotate(Q, InvParQ);

	XMVECTOR S = XMLoadA(GblScaling);
	Q /= S;

	LclLength = XMVectorGetX(XMVector3Length(Q));

	LclTranslation = Q;

	S /= XMLoadA(reference.GblScaling);
	LclScaling = S;
}


void Bone::UpdateLocalDataByPositionOnly(const Bone & reference)
{
	XMVECTOR v0 = XMLoadA(this->GblTranslation) - XMLoadA(reference.GblTranslation);
	v0 = XMVector3InverseRotate(v0, reference.GblRotation);

	LclScaling = { 1.0f, XMVectorGetX(XMVector3Length(v0)), 1.0f };

	// with Constraint No-X
	v0 = XMVector3Normalize(v0);
	XMFLOAT4A Sp;
	XMStoreFloat4A(&Sp, v0);
	float Roll = -std::asinf(Sp.x);
	float Pitch = std::atan2f(Sp.z, Sp.y);
	this->LclRotation = XMQuaternionRotationRollPitchYaw(Pitch, 0.0f, Roll);
	this->GblRotation = XMQuaternionMultiply(this->LclRotation, reference.GblRotation);
}

XMMATRIX Bone::TransformMatrix(const Bone & from, const Bone & to)
{
	using namespace DirectX;

	XMVECTOR VScaling = XMLoadA(to.GblScaling) / XMLoadA(from.GblScaling);
	XMVECTOR VRotationOrigin = XMVectorSelect(g_XMSelect1110.v, XMLoadA(from.GblTranslation), g_XMSelect1110.v);
	XMMATRIX MRotation = XMMatrixRotationQuaternion(XMQuaternionConjugate(XMLoadA(from.GblRotation)));
	XMVECTOR VTranslation = XMVectorSelect(g_XMSelect1110.v, XMLoadA(to.GblTranslation), g_XMSelect1110.v);

	VRotationOrigin = -VRotationOrigin;
	VRotationOrigin = XMVector3Transform(VRotationOrigin, MRotation);
	VRotationOrigin = VRotationOrigin * VScaling;
	XMMATRIX M = MRotation;
	M.r[3] = XMVectorSelect(g_XMIdentityR3.v, VRotationOrigin, g_XMSelect1110.v);

	MRotation = XMMatrixRotationQuaternion(XMLoadA(to.GblRotation));
	M = XMMatrixMultiply(M, MRotation);
	M.r[3] += VTranslation;

	//XMVECTOR toEnd = XMVector3Transform(XMLoadA(from.GblTranslation), M);

	//if (XMVector4Greater(XMVector3LengthSq(toEnd - XMLoadA(to.GblTranslation)), XMVectorReplicate(0.001)))
	//	std::cout << "NO!!!!" << std::endl;
	return M;
}

XMMATRIX Bone::RigidTransformMatrix(const Bone & from, const Bone & to)
{
	XMVECTOR rot = XMQuaternionInverse(from.GblRotation);
	rot = XMQuaternionMultiply(rot, to.GblRotation);
	XMVECTOR tra = XMLoadA(to.GblTranslation) - XMLoadA(from.GblTranslation);
	return XMMatrixRigidTransform(XMLoadA(from.GblTranslation), rot, tra);
}

XMDUALVECTOR Bone::RigidTransformDualQuaternion(const Bone & from, const Bone & to)
{
	XMVECTOR rot = XMQuaternionInverse(from.GblRotation);
	rot = XMQuaternionMultiply(rot, to.GblRotation);
	XMVECTOR tra = XMLoadA(to.GblTranslation) - XMLoadA(from.GblTranslation);
	return XMDualQuaternionRigidTransform(XMLoadA(from.GblTranslation), rot, tra);
}

StaticArmature::StaticArmature(gsl::array_view<JointBasicData> data)
{
	size_t jointCount = data.size();
	Joints.resize(jointCount);

	for (size_t i = 0; i < jointCount; i++)
	{
		Joints[i].SetID(i);

		int parentID = data[i].ParentID;
		if (parentID != i &&parentID >= 0)
		{
			Joints[parentID].append_children_back(&Joints[i]);
		}
		else
		{
			RootIdx = i;
		}
	}

	CaculateTopologyOrder();
}

StaticArmature::StaticArmature(std::istream & file)
{
	size_t jointCount;
	file >> jointCount;

	Joints.resize(jointCount);
	DefaultFrame.reset(new BoneHiracheryFrame(jointCount));

	// Joint line format: 
	// Hip(Name) -1(ParentID)
	// 1.5(Pitch) 2.0(Yaw) 0(Roll) 0.5(BoneLength)
	for (size_t idx = 0; idx < jointCount; idx++)
	{
		auto& joint = Joints[idx];
		auto& bone = default_frame()[idx];
		((JointBasicData&)joint).ID = idx;
		file >> ((JointBasicData&)joint).Name >> ((JointBasicData&)joint).ParentID;
		if (joint.ParentID != idx && joint.ParentID >= 0)
		{
			Joints[joint.ParentID].append_children_back(&joint);
		}
		else
		{
			RootIdx = idx;
		}

		Vector4 vec;
		file >> vec.x >> vec.y >> vec.z >> vec.w;
		bone.LclRotation = XMQuaternionRotationRollPitchYawFromVector(vec);
		bone.LclScaling.y = vec.w;
	}

	CaculateTopologyOrder();
	default_frame().RebuildGlobal(*this);
}

StaticArmature::StaticArmature(size_t JointCount, int * Parents, const char* const* Names)
	: Joints(JointCount)
{
	DefaultFrame.reset(new BoneHiracheryFrame(JointCount));
	for (size_t i = 0; i < JointCount; i++)
	{
		Joints[i].SetID(i);
		Joints[i].SetName(Names[i]);
		((JointBasicData&)Joints[i]).ParentID = Parents[i];
		if (Parents[i] != i && Parents[i] >= 0)
		{
			Joints[Parents[i]].append_children_back(&Joints[i]);
		}
		else
		{
			RootIdx = i;
		}
	}
	CaculateTopologyOrder();
}

StaticArmature::~StaticArmature()
{

}

StaticArmature::StaticArmature(self_type && rhs)
{
	*this = std::move(rhs);
}

StaticArmature::self_type & StaticArmature::operator=(self_type && rhs)
{
	using std::move;
	RootIdx = rhs.RootIdx;
	Joints = move(rhs.Joints);
	TopologyOrder = move(rhs.TopologyOrder);
	DefaultFrame = move(rhs.DefaultFrame);
	return *this;
}

//void GetBlendMatrices(_Out_ XMFLOAT4X4* pOut);

Joint * StaticArmature::at(int index) {
	return &Joints[index];
}

Joint * StaticArmature::root()
{
	return &Joints[RootIdx];
}

size_t StaticArmature::size() const
{
	return Joints.size();
}

const IArmature::frame_type & StaticArmature::default_frame() const
{
	return *DefaultFrame;
	// TODO: insert return statement here
}

void Causality::StaticArmature::set_default_frame(uptr<frame_type> && frame) { DefaultFrame = std::move(frame); }

void StaticArmature::CaculateTopologyOrder()
{
	using namespace boost;
	using namespace boost::adaptors;
	TopologyOrder.resize(size());
	copy(root()->nodes() | transformed([](const Joint& joint) {return joint.ID; }), TopologyOrder.begin());

}

// Lerp the local-rotation and scaling, "interpolate in Time"

iterator_range<std::sregex_token_iterator> words_from_string(const std::string& str)
{
	using namespace std;
	regex wordPattern("[_\\s]?[A-Za-z][a-z]*\\d*");
	sregex_token_iterator wbegin(str.begin(), str.end(), wordPattern);
	iterator_range<sregex_token_iterator> words(wbegin, sregex_token_iterator());
	return words;
}

using namespace std;

/*
namespace std
{
	//inline
	namespace literals {
		//inline
		namespace string_literals
		{
			std::string operator""s(const char* str, std::size_t len)
			{
				return std::string(str, len);
			}
		}
	}
}
*/

std::map<std::string, JointSemanticProperty>
name2semantic = boost::assign::map_list_of
(string("hand"), JointSemanticProperty(Semantic_Hand))
(string("foreleg"), JointSemanticProperty(Semantic_Hand | Semantic_Foot))
(string("arm"), JointSemanticProperty(Semantic_Hand))
(string("claw"), JointSemanticProperty(Semantic_Hand))
(string("wing"), JointSemanticProperty(Semantic_Hand | Semantic_Wing))
(string("head"), JointSemanticProperty(Semantic_Head))
(string("l"), JointSemanticProperty(Semantic_Left))
(string("r"), JointSemanticProperty(Semantic_Right))
(string("left"), JointSemanticProperty(Semantic_Left))
(string("right"), JointSemanticProperty(Semantic_Right))
(string("leg"), JointSemanticProperty(Semantic_Foot))
(string("foot"), JointSemanticProperty(Semantic_Foot))
(string("tail"), JointSemanticProperty(Semantic_Tail))
(string("ear"), JointSemanticProperty(Semantic_Ear))
(string("eye"), JointSemanticProperty(Semantic_Eye))
(string("noise"), JointSemanticProperty(Semantic_Nouse));

const JointSemanticProperty & Joint::AssignSemanticsBasedOnName()
{
	using namespace std;
	using namespace boost::adaptors;

	auto words = words_from_string(Name);
	for (auto& word : words)
	{
		string word_str;
		if (*word.first == '_' || *word.first == ' ')
			word_str = std::string(word.first + 1, word.second);
		else
			word_str = std::string(word.first, word.second);

		for (auto& c : word_str)
		{
			c = std::tolower(c);
		}

		this->Semantic |= name2semantic[word_str];
	}
	return this->Semantic;
}

const Bone & Causality::IArmature::default_bone(int index) const
{
	return default_frame()[at(index)->ID];
}

// check if two tree node is "structural-similar"
template <class Derived, bool ownnersip>
bool is_similar(_In_ const stdx::tree_node<Derived, ownnersip> *p, _In_ const stdx::tree_node<Derived, ownnersip> *q)
{
	typedef const stdx::tree_node<Derived, ownnersip> * pointer;
	pointer pc = p->first_child();
	pointer qc = q->first_child();

	if ((pc == nullptr) != (qc == nullptr))
		return false;
	else if (!(pc || qc))
		return true;

	while (pc != nullptr && qc != nullptr)
	{
		bool similar = is_similar(pc, qc);
		if (!similar) return false;
		pc = pc->next_sibling();
		qc = qc->next_sibling();
	}

	return pc == nullptr && qc == nullptr;
}

void Causality::BuildJointMirrorRelation(Joint * root, const BoneHiracheryFrame & frame)
{
	float epsilon = 1.00f;
	auto _children = root->descendants();
	std::vector<std::reference_wrapper<Joint>> children(_children.begin(), _children.end());
	std::vector<Joint*> &joints = reinterpret_cast<std::vector<Joint*> &>(children);

	for (int i = 0; i < children.size(); i++)
	{
		auto& bonei = frame[joints[i]->ID];
		auto& ti = bonei.GblTranslation;
		for (int j = i + 1; j < children.size(); j++)
		{
			auto& bonej = frame[joints[j]->ID];
			auto& tj = bonej.GblTranslation;

			if (joints[i]->parent() == joints[j]->parent() && is_similar(joints[i], joints[j]))
			{
				joints[i]->MirrorJoint = joints[j];
				joints[j]->MirrorJoint = joints[i];
			}
		}
	}
}