/*
    Lightmetrica - A modern, research-oriented renderer

    Copyright (c) 2015 Hisanari Otsu

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include <pch.h>
#include <lightmetrica/bsdf.h>
#include <lightmetrica/property.h>
#include <lightmetrica/spectrum.h>
#include <lightmetrica/surfacegeometry.h>
#include <lightmetrica/bsdfutils.h>
#include <lightmetrica/sampler.h>
#include <lightmetrica/texture.h>
#include <lightmetrica/assets.h>
#include <lightmetrica/detail/serial.h>

#define LM_COOKTORRANCE_USE_GGX 1

LM_NAMESPACE_BEGIN

class BSDF_CookTorrance final : public BSDF
{
public:

    LM_IMPL_CLASS(BSDF_CookTorrance, BSDF);

public:

    LM_IMPL_F(Load) = [this](const PropertyNode* prop, Assets* assets, const Primitive* primitive) -> bool
    {
        if (prop->Child("TexR"))
        {
            std::string id;
            prop->ChildAs("TexR", id);
            texR_  = static_cast<const Texture*>(assets->AssetByIDAndType(id, "texture", primitive));
        }
        else
        {
            R_ = SPD::FromRGB(prop->ChildAs<Vec3>("R", Vec3()));
        }

        eta_ = SPD::FromRGB(prop->ChildAs<Vec3>("eta", Vec3(0.140000_f, 0.129000_f, 0.158500_f)));
        k_ = SPD::FromRGB(prop->ChildAs<Vec3>("k", Vec3(4.586250_f, 3.348125_f, 2.329375_f)));
        roughness_ = prop->ChildAs<Float>("roughness", 0.1_f);

        return true;
    };

    LM_IMPL_F(Type) = [this]() -> int
    {
        return SurfaceInteractionType::G;
    };

    LM_IMPL_F(SampleDirection) = [this](const Vec2& u, Float uComp, int queryType, const SurfaceGeometry& geom, const Vec3& wi, Vec3& wo) -> void
    {
        const auto localWi = geom.ToLocal * wi;
        if (Math::LocalCos(localWi) <= 0_f)
        {
            return;
        }

        const auto H = SampleNormalDist(u);
        const auto localWo = -localWi - 2_f * Math::Dot(-localWi, H) * H;
        if (Math::LocalCos(localWo) <= 0_f)
        {
            return;
        }

        wo = geom.ToWorld * localWo;
    };

    LM_IMPL_F(EvaluateDirectionPDF) = [this](const SurfaceGeometry& geom, int queryType, const Vec3& wi, const Vec3& wo, bool evalDelta) -> PDFVal
    {
        const auto localWi = geom.ToLocal * wi;
        const auto localWo = geom.ToLocal * wo;
        if (Math::LocalCos(localWi) <= 0_f || Math::LocalCos(localWo) <= 0_f)
        {
            return PDFVal(PDFMeasure::ProjectedSolidAngle, 0_f);
        }

        const auto H = Math::Normalize(localWi + localWo);
        const Float D = EvaluateNormalDist(H);
        return PDFVal(PDFMeasure::ProjectedSolidAngle, D * Math::LocalCos(H) / (4_f * Math::Dot(localWo, H)) / Math::LocalCos(localWo));
    };

    LM_IMPL_F(EvaluateDirection) = [this](const SurfaceGeometry& geom, int types, const Vec3& wi, const Vec3& wo, TransportDirection transDir, bool evalDelta) -> SPD
    {
        const auto localWi = geom.ToLocal * wi;
        const auto localWo = geom.ToLocal * wo;
        if (Math::LocalCos(localWi) <= 0_f || Math::LocalCos(localWo) <= 0_f)
        {
            return SPD();
        }

        const auto  H = Math::Normalize(localWi + localWo);
        const Float D = EvaluateNormalDist(H);
        const Float G = EvalauteShadowMaskingFunc(localWi, localWo, H);
        const auto  F = EvaluateFrConductor(Math::Dot(localWi, H));
        const auto  R = texR_ ? SPD::FromRGB(texR_->Evaluate(geom.uv)) : R_;
        return R * D * G * F / (4_f * Math::LocalCos(localWi)) / Math::LocalCos(localWo) * BSDFUtils::ShadingNormalCorrection(geom, wi, wo, transDir);
    };

    LM_IMPL_F(IsDeltaDirection) = [this](int type) -> bool
    {
        return false;
    };

    LM_IMPL_F(IsDeltaPosition) = [this](int type) -> bool
    {
        return false;
    };

    LM_IMPL_F(Serialize) = [this](std::ostream& stream) -> bool
    {
        {
            cereal::PortableBinaryOutputArchive oa(stream);
            int texID = texR_ ? texR_->Index() : -1;
            oa(R_, texID, eta_, roughness_);
        }
        return true;
    };

    LM_IMPL_F(Deserialize) = [this](std::istream& stream, const std::unordered_map<std::string, void*>& userdata) -> bool
    {
        int texID;
        {
            cereal::PortableBinaryInputArchive ia(stream);
            ia(R_, texID, eta_, roughness_);
        }
        if (texID >= 0)
        {
            auto* assets = static_cast<Assets*>(userdata.at("assets"));
            texR_ = static_cast<const Texture*>(assets->GetByIndex(texID));
        }
        return true;
    };

    LM_IMPL_F(Glossiness) = [this]() -> Float
    {
        return roughness_;
    };

    LM_IMPL_F(Reflectance) = [this]() -> SPD { return R_; };
    LM_IMPL_F(Reflectance2) = [this](const SurfaceGeometry& geom) -> SPD { return texR_ ? SPD::FromRGB(texR_->Evaluate(geom.uv)) : R_; };

private:

    auto EvaluateNormalDist(const Vec3& H) const -> Float
    {
        #if LM_COOKTORRANCE_USE_GGX
        return EvaluateGGX(H);
        #else
        return EvaluateBechmannDist(H);
        #endif
    }

    auto SampleNormalDist(const Vec2& u) const -> Vec3
    {
        #if LM_COOKTORRANCE_USE_GGX
        return SampleGGX(u);
        #else
        return SampleBechmannDist(u);
        #endif
    }

private:

    auto EvaluateGGX(const Vec3& H) const -> Float
    {
        const auto cosH = Math::LocalCos(H);
        const auto tanH = Math::LocalTan(H);
        if (cosH <= 0_f) return 0_f;
        const Float t1 = roughness_ * roughness_;
        const Float t2 = [&]() {
            const Float t = roughness_ * roughness_ + tanH * tanH;
            return Math::Pi() * cosH * cosH * cosH * cosH * t * t;
        }();
        return t1 / t2;
    }

    auto SampleGGX(const Vec2& u) const -> Vec3
    {
        // Input u \in [0,1]^2
        const auto ToOpenOpen = [](Float u) -> Float { return (1_f - 2_f * Math::Eps()) * u + Math::Eps(); };
        //const auto ToClosedOpen = [](Float u) -> Float { return (1_f - Math::Eps()) * u; };
        const auto ToOpenClosed = [](Float u) -> Float { return (1_f - Math::Eps()) * u + Math::Eps(); };

        // u0 \in (0,1]
        // u1 \in (0,1)
        const auto u0 = ToOpenClosed(u[0]);
        const auto u1 = ToOpenOpen(u[1]);

        // Robust way of computation
        const auto cosTheta = [&]() -> Float {
            const auto v1 = Math::Sqrt(1_f - u0);
            const auto v2 = Math::Sqrt(1_f - (1_f - roughness_ * roughness_) * u0);
            return v1 / v2;
        }();
        const auto sinTheta = [&]() -> Float {
            const auto v1 = Math::Sqrt(u0);
            const auto v2 = Math::Sqrt(1_f - (1_f - roughness_ * roughness_) * u0);
            return roughness_ * (v1 / v2);
        }();
        const auto phi = Math::Pi() * (2_f * u1 - 1_f);
        return Vec3(sinTheta * Math::Cos(phi), sinTheta * Math::Sin(phi), cosTheta);
    }

private:

    //const auto SampleBechmannDist = [this](const Vec2& u) -> Vec3
    //{
    //    const Float tanThetaHSqr = -roughness_ * roughness_ * std::log(1_f - u[0]);
    //    const Float cosThetaH = 1_f / Math::Sqrt(1_f + tanThetaHSqr);
    //    const Float cosThetaH2 = cosThetaH * cosThetaH;
    //    Float sinThetaH = Math::Sqrt(Math::Max(0_f, 1_f - cosThetaH2));
    //    Float phiH = 2_f * Math::Pi() * u[1];
    //    return Vec3(sinThetaH * Math::Cos(phiH), sinThetaH * Math::Sin(phiH), cosThetaH);
    //};

    auto EvaluateBechmannDist(const Vec3& H) const -> Float
    {
        if (Math::LocalCos(H) <= 0_f) return 0_f;
        const Float ex = Math::LocalTan(H) / roughness_;
        const Float t1 = std::exp(-(ex * ex));
        const Float t2 = (Math::Pi() * roughness_ * roughness_ * std::pow(Math::LocalCos(H), 4_f));
        return t1 / t2;
    }

    auto SampleBechmannDist(const Vec2& u) const -> Vec3
    {
        const Float cosThetaH = [&]() -> Float
        {
            // Handling nasty numerical error
            if (1_f - u[0] < Math::Eps()) return 0_f;
            const Float tanThetaHSqr = -roughness_ * roughness_ * std::log(1_f - u[0]);
            return 1_f / Math::Sqrt(1_f + tanThetaHSqr);
        }();
        const Float cosThetaH2 = cosThetaH * cosThetaH;
        Float sinThetaH = Math::Sqrt(Math::Max(0_f, 1_f - cosThetaH2));
        Float phiH = 2_f * Math::Pi() * u[1];
        return Vec3(sinThetaH * Math::Cos(phiH), sinThetaH * Math::Sin(phiH), cosThetaH);
    };

private:

    #if 0
    auto EvaluatePhongDist(const Vec3& H) const -> Float
    {
        const Float Coeff = std::tgamma((roughness_ + 3_f) * .5_f) / std::tgamma((roughness_ + 2_f) * .5_f) / std::sqrt(Math::Pi());
        if (Math::LocalCos(H) <= 0_f) return 0_f;
        return std::pow(Math::LocalCos(H), roughness_) * Coeff;
    }
    #endif

private:

    auto EvalauteShadowMaskingFunc(const Vec3& wi, const Vec3& wo, const Vec3& H) const -> Float
    {
        const Float n_dot_H = Math::LocalCos(H);
        const Float n_dot_wo = Math::LocalCos(wo);
        const Float n_dot_wi = Math::LocalCos(wi);
        const Float wo_dot_H = std::abs(Math::Dot(wo, H));
        const Float wi_dot_H = std::abs(Math::Dot(wo, H));
        return std::min(1_f, std::min(2_f * n_dot_H * n_dot_wo / wo_dot_H, 2_f * n_dot_H * n_dot_wi / wi_dot_H));
    }

    auto EvaluateFrConductor(Float cosThetaI) const -> SPD
    {
        const auto tmp = (eta_*eta_ + k_*k_) * (cosThetaI * cosThetaI);
        const auto rParl2 = (tmp - (eta_ * SPD(2_f * cosThetaI)) + SPD(1_f)) / (tmp + (eta_ * SPD(2_f * cosThetaI)) + SPD(1_f));
        const auto tmpF = eta_*eta_ + k_*k_;
        const auto rPerp2 = (tmpF - (eta_ * (2_f * cosThetaI)) + cosThetaI*cosThetaI) / (tmpF + (eta_ * (2_f * cosThetaI)) + cosThetaI*cosThetaI);
        return (rParl2 + rPerp2) * .5_f;
    }

public:

    SPD R_;
    const Texture* texR_ = nullptr;
    SPD eta_;
    SPD k_;
    Float roughness_;

};

LM_COMPONENT_REGISTER_IMPL(BSDF_CookTorrance, "bsdf::cook_torrance");

LM_NAMESPACE_END
