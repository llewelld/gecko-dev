/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_EmbedLiteCompositorParent_h
#define mozilla_layers_EmbedLiteCompositorParent_h

#define COMPOSITOR_PERFORMANCE_WARNING

#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/CompositorChild.h"
#include "Layers.h"
#include "EmbedLiteViewThreadParent.h"

namespace mozilla {
namespace layers {
class CompositingRenderTarget;
}
namespace embedlite {

class EmbedLiteCompositorParent : public mozilla::layers::CompositorParent
{
public:
  EmbedLiteCompositorParent(nsIWidget* aWidget,
                            bool aRenderToEGLSurface,
                            int aSurfaceWidth, int aSurfaceHeight,
                            uint32_t id);

  bool RenderToContext(gfx::DrawTarget* aTarget);
  bool RenderGL();
  void SetSurfaceSize(int width, int height);
  void SetWorldTransform(gfx::Matrix);
  void SetClipping(const gfxRect& aClipRect);
  void SetWorldOpacity(float aOpacity);
  void* GetPlatformImage(int* width, int* height);

  virtual void SetChildCompositor(mozilla::layers::CompositorChild*);
  mozilla::layers::CompositorChild* GetChildCompositor() {
    return mChildCompositor;
  }
  virtual bool RequestHasHWAcceleratedContext();
private:
  virtual ~EmbedLiteCompositorParent();

protected:
  virtual PLayerTransactionParent*
    AllocPLayerTransactionParent(const nsTArray<LayersBackend>& aBackendHints,
                                 const uint64_t& aId,
                                 TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                 bool* aSuccess);

  virtual void ScheduleTask(CancelableTask*, int);

  bool IsGLBackend();

  RefPtr<mozilla::layers::CompositorChild> mChildCompositor;
  uint32_t mId;
  gfx::Matrix mWorldTransform;
  nsIntRect mActiveClipping;
  CancelableTask *mCurrentCompositeTask;
  float mWorldOpacity;
  gfx::IntSize mLastViewSize;
  short mInitialPaintCount;
};

} // embedlite
} // mozilla

#endif // mozilla_layers_EmbedLiteCompositorParent_h
