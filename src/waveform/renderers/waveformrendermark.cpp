#include "waveform/renderers/waveformrendermark.h"

#include <QDomNode>
#include <QPainter>
#include <QPainterPath>

#include "control/controlproxy.h"
#include "engine/controls/cuecontrol.h"
#include "moc_waveformrendermark.cpp"
#include "track/track.h"
#include "util/color/color.h"
#include "util/painterscope.h"
#include "waveform/renderers/waveformwidgetrenderer.h"
#include "waveform/waveform.h"
#include "widget/wskincolor.h"
#include "widget/wwidget.h"

WaveformRenderMark::WaveformRenderMark(
        WaveformWidgetRenderer* waveformWidgetRenderer) :
    WaveformRendererAbstract(waveformWidgetRenderer) {
}

void WaveformRenderMark::setup(const QDomNode& node, const SkinContext& context) {
    WaveformSignalColors signalColors = *m_waveformRenderer->getWaveformSignalColors();
    m_marks.setup(m_waveformRenderer->getGroup(), node, context, signalColors);
}

void WaveformRenderMark::draw(QPainter* painter, QPaintEvent* /*event*/) {
    PainterScope PainterScope(painter);
    // Maps mark objects to their positions in the widget.
    QMap<WaveformMarkPointer, int> marksOnScreen;
    /*
    //DEBUG
    for (int i = 0; i < m_markPoints.size(); i++) {
        if (m_waveformWidget->getTrackSamples())
            painter->drawText(40*i,12+12*(i%3),QString::number(m_markPoints[i]->get() / (double)m_waveformWidget->getTrackSamples()));
    }
    */

    painter->setWorldMatrixEnabled(false);

    for (const auto& pMark : m_marks) {
        if (!pMark->isValid()) {
            continue;
        }

        if (pMark->hasVisible() && !pMark->isVisible()) {
            continue;
        }

        // Generate image on first paint can't be done in setup since we need to
        // wait for the render widget to be resized yet.
        if (pMark->m_needsUpdate || pMark->m_image.isNull()) {
            generateMarkImage(pMark);
        }

        const double samplePosition = pMark->getSamplePosition();
        if (samplePosition != Cue::kNoPosition) {
            const double currentMarkPoint =
                    m_waveformRenderer->transformSamplePositionInRendererWorld(samplePosition);
            const double sampleEndPosition = pMark->getSampleEndPosition();
            if (m_waveformRenderer->getOrientation() == Qt::Horizontal) {
                // Pixmaps are expected to have the mark stroke at the center,
                // and preferably have an odd width in order to have the stroke
                // exactly at the sample position.
                const int markHalfWidth =
                        static_cast<int>(pMark->m_image.width() / 2.0 /
                                m_waveformRenderer->getDevicePixelRatio());
                const int drawOffset = static_cast<int>(currentMarkPoint) - markHalfWidth;

                bool visible = false;
                // Check if the current point needs to be displayed.
                if (currentMarkPoint > -markHalfWidth && currentMarkPoint < m_waveformRenderer->getWidth() + markHalfWidth) {
                    painter->drawImage(drawOffset, 0, pMark->m_image);
                    visible = true;
                }

                // Check if the range needs to be displayed.
                if (samplePosition != sampleEndPosition && sampleEndPosition != Cue::kNoPosition) {
                    DEBUG_ASSERT(samplePosition < sampleEndPosition);
                    const double currentMarkEndPoint =
                            m_waveformRenderer->transformSamplePositionInRendererWorld(
                                    sampleEndPosition);
                    if (visible || currentMarkEndPoint > 0) {
                        QColor color = pMark->fillColor();
                        color.setAlphaF(0.4);

                        QLinearGradient gradient(QPointF(0, 0),
                                QPointF(0, m_waveformRenderer->getHeight()));
                        gradient.setColorAt(0, color);
                        gradient.setColorAt(0.25, QColor(Qt::transparent));
                        gradient.setColorAt(0.75, QColor(Qt::transparent));
                        gradient.setColorAt(1, color);
                        painter->fillRect(
                                QRectF(QPointF(currentMarkPoint, 0),
                                        QPointF(currentMarkEndPoint,
                                                m_waveformRenderer
                                                        ->getHeight())),
                                QBrush(gradient));
                        visible = true;
                    }
                }

                if (visible) {
                    marksOnScreen[pMark] = drawOffset;
                }
            } else {
                const int markHalfHeight = static_cast<int>(pMark->m_image.height() / 2.0);
                const int drawOffset = static_cast<int>(currentMarkPoint) - markHalfHeight;

                bool visible = false;
                // Check if the current point needs to be displayed.
                if (currentMarkPoint > -markHalfHeight &&
                        currentMarkPoint < m_waveformRenderer->getHeight() +
                                        markHalfHeight) {
                    painter->drawImage(drawOffset, 0, pMark->m_image);
                    visible = true;
                }

                // Check if the range needs to be displayed.
                if (samplePosition != sampleEndPosition && sampleEndPosition != Cue::kNoPosition) {
                    DEBUG_ASSERT(samplePosition < sampleEndPosition);
                    double currentMarkEndPoint =
                            m_waveformRenderer
                                    ->transformSamplePositionInRendererWorld(
                                            sampleEndPosition);
                    if (currentMarkEndPoint < m_waveformRenderer->getHeight()) {
                        QColor color = pMark->fillColor();
                        color.setAlphaF(0.4);

                        QLinearGradient gradient(QPointF(0, 0),
                                QPointF(m_waveformRenderer->getWidth(), 0));
                        gradient.setColorAt(0, color);
                        gradient.setColorAt(0.25, QColor(Qt::transparent));
                        gradient.setColorAt(0.75, QColor(Qt::transparent));
                        gradient.setColorAt(1, color);
                        painter->fillRect(
                                QRectF(QPointF(0, currentMarkPoint),
                                        QPointF(m_waveformRenderer->getWidth(),
                                                currentMarkEndPoint)),
                                QBrush(gradient));
                        visible = true;
                    }
                }

                if (visible) {
                    marksOnScreen[pMark] = drawOffset;
                }
            }
        }
    }
    m_waveformRenderer->setMarkPositions(marksOnScreen);
}

void WaveformRenderMark::onResize() {
    // Flag that the mark image has to be updated. New images will be created on next paint.
    for (const auto& pMark : m_marks) {
        pMark->m_needsUpdate = true;
    }
}

void WaveformRenderMark::onSetTrack() {
    slotCuesUpdated();

    const TrackPointer pTrackInfo = m_waveformRenderer->getTrackInfo();
    if (!pTrackInfo) {
        return;
    }
    connect(pTrackInfo.get(),
            &Track::cuesUpdated,
            this,
            &WaveformRenderMark::slotCuesUpdated);
}

void WaveformRenderMark::slotCuesUpdated() {
    const TrackPointer pTrackInfo = m_waveformRenderer->getTrackInfo();
    if (!pTrackInfo) {
        return;
    }

    QList<CuePointer> loadedCues = pTrackInfo->getCuePoints();
    for (const CuePointer& pCue : loadedCues) {
        int hotCue = pCue->getHotCue();
        if (hotCue == Cue::kNoHotCue) {
            continue;
        }

        // Here we assume no two cues can have the same hotcue assigned,
        // because WaveformMarkSet stores one mark for each hotcue.
        WaveformMarkPointer pMark = m_marks.getHotCueMark(hotCue);
        if (pMark.isNull()) {
            continue;
        }

        QString newLabel = pCue->getLabel();
        QColor newColor = mixxx::RgbColor::toQColor(pCue->getColor());
        if (pMark->m_text.isNull() || newLabel != pMark->m_text ||
                !pMark->fillColor().isValid() ||
                newColor != pMark->fillColor()) {
            pMark->m_text = newLabel;
            int dimBrightThreshold = m_waveformRenderer->getDimBrightThreshold();
            pMark->setBaseColor(newColor, dimBrightThreshold);
            generateMarkImage(pMark);
        }
    }
}

void WaveformRenderMark::generateMarkImage(WaveformMarkPointer pMark) {
    pMark->m_image = pMark->generateImage(m_waveformRenderer->getBreadth(),
            m_waveformRenderer->getDevicePixelRatio());
    pMark->m_needsUpdate = false;
}
