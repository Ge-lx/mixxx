#pragma once

#include "rendergraph/openglnode.h"
#include "shaders/rgbshader.h"
#include "util/class.h"
#include "waveform/renderers/allshader/rgbdata.h"
#include "waveform/renderers/allshader/vertexdata.h"
#include "waveform/renderers/allshader/waveformrenderersignalbase.h"

namespace allshader {
class WaveformRendererRGB;
}

class allshader::WaveformRendererRGB final
        : public allshader::WaveformRendererSignalBase,
          public rendergraph::OpenGLNode {
  public:
    explicit WaveformRendererRGB(WaveformWidgetRenderer* waveformWidget,
            ::WaveformRendererAbstract::PositionSource type =
                    ::WaveformRendererAbstract::Play,
            WaveformRendererSignalBase::Options options = WaveformRendererSignalBase::Option::None);

    // override ::WaveformRendererSignalBase
    void onSetup(const QDomNode& node) override;

    void initializeGL() override;
    void paintGL() override;

    bool supportsSlip() const override {
        return true;
    }

  private:
    mixxx::RGBShader m_shader;
    VertexData m_vertices;
    RGBData m_colors;

    bool m_isSlipRenderer;
    WaveformRendererSignalBase::Options m_options;

    DISALLOW_COPY_AND_ASSIGN(WaveformRendererRGB);
};
