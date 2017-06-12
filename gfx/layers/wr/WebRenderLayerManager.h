/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERLAYERMANAGER_H
#define GFX_WEBRENDERLAYERMANAGER_H

#include "Layers.h"
#include "mozilla/MozPromise.h"
#include "mozilla/layers/APZTestData.h"
#include "mozilla/layers/TransactionIdAllocator.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/AnimationHelper.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderAPI.h"

class nsIWidget;
class nsDisplayList;

namespace mozilla {
namespace layers {

class CompositorBridgeChild;
class KnowsCompositor;
class PCompositorBridgeChild;
class WebRenderBridgeChild;

class WMAnimationData
{
public:
  NS_INLINE_DECL_REFCOUNTING(TransactionIdAllocator)

  typedef InfallibleTArray<Animation> AnimationArray;

  WMAnimationData()
  : mAnimationGeneration(0),
    mCompositorAnimationsId(0)
  {
  }

  Animation* AddAnimation()
  {
    // Here generates a new id when the first animation is added and
    // this id is used to represent the animations in this layer.
    //EnsureAnimationsId();
    if (!mCompositorAnimationsId) {
      mCompositorAnimationsId = AnimationHelper::GetNextCompositorAnimationsId();
    }

    MOZ_ASSERT(!mPendingAnimations, "should have called ClearAnimations first");

    Animation* anim = mAnimations.AppendElement();

    //Mutated();
    return anim;
  }

  Animation* AddAnimationForNextTransaction()
  {
    MOZ_ASSERT(mPendingAnimations,
               "should have called ClearAnimationsForNextTransaction first");

    Animation* anim = mPendingAnimations->AppendElement();

    return anim;
  }

  bool HasTransformAnimation() const
  {
    for (uint32_t i = 0; i < mAnimations.Length(); i++) {
      if (mAnimations[i].property() == eCSSProperty_transform) {
        return true;
      }
    }
    return false;
  }

  AnimationArray& GetAnimations() { return mAnimations; }

  void UpdateTransformDataForAnimation()
  {
    for (Animation& animation : mAnimations) {
      if (animation.property() == eCSSProperty_transform) {
        // TODO
        //TransformData& transformData = animation.data().get_TransformData();
        //transformData.inheritedXScale() = GetInheritedXScale();
        //transformData.inheritedYScale() = GetInheritedYScale();
        //transformData.hasPerspectiveParent() =
          //GetParent() && GetParent()->GetTransformIsPerspective();
      }
    }
  }

  uint64_t mAnimationGeneration;
  uint64_t mCompositorAnimationsId;
  AnimationArray mAnimations;
  nsAutoPtr<AnimationArray> mPendingAnimations;
};

class WMImageData
{
public:
  NS_INLINE_DECL_REFCOUNTING(TransactionIdAllocator)

  wr::MaybeExternalImageId mExternalImageId;
  Maybe<wr::ImageKey> mKey;
  RefPtr<ImageClient> mImageClient;
  CompositableType mImageClientTypeContainer;
  Maybe<wr::PipelineId> mPipelineId;
};

class WMItemData
{
public:
  NS_INLINE_DECL_REFCOUNTING(TransactionIdAllocator)

  RefPtr<WMAnimationData> mAnimationData;
  RefPtr<WMImageData> mImageData;
};

class WebRenderLayerManager final : public LayerManager
{
  typedef nsTArray<RefPtr<Layer> > LayerRefArray;

public:
  explicit WebRenderLayerManager(nsIWidget* aWidget);
  void Initialize(PCompositorBridgeChild* aCBChild, wr::PipelineId aLayersId, TextureFactoryIdentifier* aTextureFactoryIdentifier);

  virtual void Destroy() override;

protected:
  virtual ~WebRenderLayerManager();

public:
  virtual KnowsCompositor* AsKnowsCompositor() override;
  WebRenderLayerManager* AsWebRenderLayerManager() override { return this; }
  virtual CompositorBridgeChild* GetCompositorBridgeChild() override;

  virtual int32_t GetMaxTextureSize() const override;

  virtual bool BeginTransactionWithTarget(gfxContext* aTarget) override;
  virtual bool BeginTransaction() override;
  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override;
  Maybe<wr::ImageKey> CreateImageKey(nsDisplayItem* aItem,
                                     ImageContainer* aContainer,
                                     mozilla::wr::DisplayListBuilder& aBuilder,
                                     const StackingContextHelper& aSc,
                                     gfx::IntSize& aSize);
  bool PushImage(nsDisplayItem* aItem,
                 mozilla::wr::DisplayListBuilder& aBuilder,
                 const StackingContextHelper& aSc,
                 ImageContainer* aContainer);

  static void CreateWebRenderCommandsFromDisplayList(WebRenderLayerManager* aManager,
                                                     nsDisplayList* aDisplayList,
                                                     nsDisplayListBuilder* aDisplayListBuilder,
                                                     StackingContextHelper& aSc,
                                                     wr::DisplayListBuilder& aBuilder);
  void EndTransactionWithoutLayer(nsDisplayList* aDisplayList,
                                  nsDisplayListBuilder* aDisplayListBuilder);
  //void AddAnimation(nsDisplayItem* aItem);

  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) override;

  virtual LayersBackend GetBackendType() override { return LayersBackend::LAYERS_WR; }
  virtual void GetBackendName(nsAString& name) override { name.AssignLiteral("WebRender"); }
  virtual const char* Name() const override { return "WebRender"; }

  virtual void SetRoot(Layer* aLayer) override;

  virtual already_AddRefed<PaintedLayer> CreatePaintedLayer() override;
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() override;
  virtual already_AddRefed<ImageLayer> CreateImageLayer() override;
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() override;
  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer() override;
  virtual already_AddRefed<ColorLayer> CreateColorLayer() override;
  virtual already_AddRefed<RefLayer> CreateRefLayer() override;
  virtual already_AddRefed<TextLayer> CreateTextLayer() override;
  virtual already_AddRefed<BorderLayer> CreateBorderLayer() override;
  virtual already_AddRefed<DisplayItemLayer> CreateDisplayItemLayer() override;

  virtual bool NeedsWidgetInvalidation() override { return false; }

  virtual void SetLayerObserverEpoch(uint64_t aLayerObserverEpoch) override;

  virtual void DidComposite(uint64_t aTransactionId,
                            const mozilla::TimeStamp& aCompositeStart,
                            const mozilla::TimeStamp& aCompositeEnd) override;

  virtual void ClearCachedResources(Layer* aSubtree = nullptr) override;
  virtual void UpdateTextureFactoryIdentifier(const TextureFactoryIdentifier& aNewIdentifier,
                                              uint64_t aDeviceResetSeqNo) override;
  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() override;

  virtual void SetTransactionIdAllocator(TransactionIdAllocator* aAllocator) override
  { mTransactionIdAllocator = aAllocator; }

  virtual void AddDidCompositeObserver(DidCompositeObserver* aObserver) override;
  virtual void RemoveDidCompositeObserver(DidCompositeObserver* aObserver) override;

  virtual void FlushRendering() override;
  virtual void WaitOnTransactionProcessed() override;

  virtual void SendInvalidRegion(const nsIntRegion& aRegion) override;

  virtual void Composite() override;

  virtual void SetNeedsComposite(bool aNeedsComposite) override
  {
    mNeedsComposite = aNeedsComposite;
  }
  virtual bool NeedsComposite() const override { return mNeedsComposite; }
  virtual void SetIsFirstPaint() override { mIsFirstPaint = true; }

  bool AsyncPanZoomEnabled() const override;

  DrawPaintedLayerCallback GetPaintedLayerCallback() const
  { return mPaintedLayerCallback; }

  void* GetPaintedLayerCallbackData() const
  { return mPaintedLayerCallbackData; }

  // adds an imagekey to a list of keys that will be discarded on the next
  // transaction or destruction
  void AddImageKeyForDiscard(wr::ImageKey);
  void DiscardImages();
  void DiscardLocalImages();

  // Before destroying a layer with animations, add its compositorAnimationsId
  // to a list of ids that will be discarded on the next transaction
  void AddCompositorAnimationsIdForDiscard(uint64_t aId);
  void DiscardCompositorAnimations();

  WebRenderBridgeChild* WrBridge() const { return mWrChild; }

  virtual void Mutated(Layer* aLayer) override;
  virtual void MutatedSimple(Layer* aLayer) override;

  void Hold(Layer* aLayer);
  void SetTransactionIncomplete() { mTransactionIncomplete = true; }
  bool IsMutatedLayer(Layer* aLayer);

  // See equivalent function in ClientLayerManager
  void LogTestDataForCurrentPaint(FrameMetrics::ViewID aScrollId,
                                  const std::string& aKey,
                                  const std::string& aValue) {
    mApzTestData.LogTestDataForPaint(mPaintSequenceNumber, aScrollId, aKey, aValue);
  }
  // See equivalent function in ClientLayerManager
  const APZTestData& GetAPZTestData() const
  { return mApzTestData; }

  void SendAnimationData(WMAnimationData* aAnimationData,
                         OptionalOpacity aOpacity,
                         OptionalTransform aTransform);
  already_AddRefed<WMAnimationData> CreateOrRecycleAnimationData(nsDisplayItem* aItem);
  already_AddRefed<WMImageData> CreateOrRecycleImageData(nsDisplayItem* aItem);

private:
  /**
   * Take a snapshot of the parent context, and copy
   * it into mTarget.
   */
  void MakeSnapshotIfRequired(LayoutDeviceIntSize aSize);

  void ClearLayer(Layer* aLayer);

  bool EndTransactionInternal(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags);

  std::map<nsIFrame*, std::map<uint32_t, RefPtr<WMItemData>>> mItemData;
  std::map<nsIFrame*, std::map<uint32_t, RefPtr<WMItemData>>> mLastItemData;

private:
  nsIWidget* MOZ_NON_OWNING_REF mWidget;
  std::vector<wr::ImageKey> mImageKeys;
  nsTArray<uint64_t> mDiscardedCompositorAnimationsIds;

  /* PaintedLayer callbacks; valid at the end of a transaciton,
   * while rendering */
  DrawPaintedLayerCallback mPaintedLayerCallback;
  void *mPaintedLayerCallbackData;

  RefPtr<WebRenderBridgeChild> mWrChild;

  RefPtr<TransactionIdAllocator> mTransactionIdAllocator;
  uint64_t mLatestTransactionId;

  nsTArray<DidCompositeObserver*> mDidCompositeObservers;

  LayerRefArray mKeepAlive;

  // Layers that have been mutated. If we have an empty transaction
  // then a display item layer will no longer be valid
  // if it was a mutated layers.
  void AddMutatedLayer(Layer* aLayer);
  void ClearMutatedLayers();
  LayerRefArray mMutatedLayers;
  bool mTransactionIncomplete;

  bool mNeedsComposite;
  bool mIsFirstPaint;

 // When we're doing a transaction in order to draw to a non-default
 // target, the layers transaction is only performed in order to send
 // a PLayers:Update.  We save the original non-default target to
 // mTarget, and then perform the transaction. After the transaction ends,
 // we send a message to our remote side to capture the actual pixels
 // being drawn to the default target, and then copy those pixels
 // back to mTarget.
 RefPtr<gfxContext> mTarget;

  // See equivalent field in ClientLayerManager
  uint32_t mPaintSequenceNumber;
  // See equivalent field in ClientLayerManager
  APZTestData mApzTestData;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_WEBRENDERLAYERMANAGER_H */
