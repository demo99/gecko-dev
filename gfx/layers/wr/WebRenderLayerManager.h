/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERLAYERMANAGER_H
#define GFX_WEBRENDERLAYERMANAGER_H

#include "Layers.h"
#include "mozilla/MozPromise.h"
#include "mozilla/layers/APZTestData.h"
#include "mozilla/layers/FocusTarget.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/TransactionIdAllocator.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsDisplayList.h"

class nsIWidget;

namespace mozilla {
namespace layers {

class CompositorBridgeChild;
class KnowsCompositor;
class PCompositorBridgeChild;
class WebRenderBridgeChild;
class WebRenderParentCommand;

typedef Pair<nsIFrame*, uint32_t> FrameDisplayItemKey;

/**
 * A hash entry that combines frame pointer and display item's per frame key.
 */
class FrameDisplayItemKeyHashEntry : public PLDHashEntryHdr
{
public:
  typedef FrameDisplayItemKey KeyType;
  typedef const FrameDisplayItemKey* KeyTypePointer;

  explicit FrameDisplayItemKeyHashEntry(KeyTypePointer aKey)
    : mKey(*aKey) { }
  explicit FrameDisplayItemKeyHashEntry(const FrameDisplayItemKeyHashEntry& aCopy) = default;

  ~FrameDisplayItemKeyHashEntry() = default;

  KeyType GetKey() const { return mKey; }
  bool KeyEquals(KeyTypePointer aKey) const
  {
    return mKey.first() == aKey->first() && mKey.second() == aKey->second();
  }

  static KeyTypePointer KeyToPointer(KeyType& aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey)
  {
    if (!aKey)
      return 0;

    return HashGeneric(aKey->first(), aKey->second());
  }

  enum { ALLOW_MEMMOVE = true };

  FrameDisplayItemKey mKey;
};

class WebRenderImageData;

class WebRenderUserData
{
public:
  NS_INLINE_DECL_REFCOUNTING(WebRenderUserData)

  explicit WebRenderUserData(WebRenderLayerManager* aWRManager)
    : mWRManager(aWRManager)
  { }

  virtual WebRenderImageData* AsImageData() { return nullptr; }

  enum class TYPE {
    IMAGE,
  };

  virtual TYPE GetType() = 0;

protected:
  virtual ~WebRenderUserData() {}

  WebRenderBridgeChild* WrBridge() const;

  WebRenderLayerManager* mWRManager;
};

class WebRenderImageData : public WebRenderUserData
{
public:
  explicit WebRenderImageData(WebRenderLayerManager* aWRManager);
  virtual ~WebRenderImageData();

  virtual WebRenderImageData* AsImageData() override { return this; }
  virtual TYPE GetType() override { return TYPE::IMAGE; }
  static TYPE Type() { return TYPE::IMAGE; }

  Maybe<wr::ImageKey> UpdateImageKey(ImageContainer* aContainer);

  void CreateAsyncImageWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                         ImageContainer* aContainer,
                                         const StackingContextHelper& aSc,
                                         const LayerRect& aBounds,
                                         const LayerRect& aSCBounds,
                                         const gfx::Matrix4x4& aSCTransform,
                                         const gfx::MaybeIntSize& aScaleToSize,
                                         const WrImageRendering& aFilter,
                                         const WrMixBlendMode& aMixBlendMode);

protected:
  void CreateImageClientIfNeeded();
  void CreateExternalImageIfNeeded();

  wr::MaybeExternalImageId mExternalImageId;
  Maybe<wr::ImageKey> mKey;
  RefPtr<ImageClient> mImageClient;
  Maybe<wr::PipelineId> mPipelineId;
  RefPtr<ImageContainer> mContainer;
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
                 ImageContainer* aContainer,
                 mozilla::wr::DisplayListBuilder& aBuilder,
                 const StackingContextHelper& aSc,
                 const LayerRect& aRect);
  bool PushItemAsBlobImage(nsDisplayItem* aItem,
                           wr::DisplayListBuilder& aBuilder,
                           const StackingContextHelper& aSc,
                           nsDisplayListBuilder* aDisplayListBuilder);
  void CreateWebRenderCommandsFromDisplayList(nsDisplayList* aDisplayList,
                                              nsDisplayListBuilder* aDisplayListBuilder,
                                              StackingContextHelper& aSc,
                                              wr::DisplayListBuilder& aBuilder);
  void EndTransactionWithoutLayer(nsDisplayList* aDisplayList,
                                  nsDisplayListBuilder* aDisplayListBuilder);
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

  virtual void ScheduleComposite() override;

  virtual void SetNeedsComposite(bool aNeedsComposite) override
  {
    mNeedsComposite = aNeedsComposite;
  }
  virtual bool NeedsComposite() const override { return mNeedsComposite; }
  virtual void SetIsFirstPaint() override { mIsFirstPaint = true; }
  virtual void SetFocusTarget(const FocusTarget& aFocusTarget) override;

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

  template<class T> already_AddRefed<T>
  CreateOrRecycleWebRenderUserData(nsDisplayItem* aItem)
  {
    MOZ_ASSERT(aItem);
    FrameDisplayItemKey key = MakePair(aItem->Frame(), aItem->GetPerFrameKey());

    RefPtr<WebRenderUserData> data;

    if (auto entry = mItemData.Lookup(key)) {
      data = entry.Data();
    } else if (auto entry = mLastItemData.Lookup(key)) {
      data = entry.Data();
      mItemData.Put(key, data);
      entry.Remove();
    }

    if (!data || (data->GetType() != T::Type())) {
      data = new T(this);
      mItemData.Put(key, data);
    }

    MOZ_ASSERT(data);
    MOZ_ASSERT(data->GetType() == T::Type());

    RefPtr<T> res = static_cast<T*>(data.get());
    return res.forget();
  }

private:
  /**
   * Take a snapshot of the parent context, and copy
   * it into mTarget.
   */
  void MakeSnapshotIfRequired(LayoutDeviceIntSize aSize);

  void ClearLayer(Layer* aLayer);

  bool EndTransactionInternal(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags,
                              nsDisplayList* aDisplayList = nullptr,
                              nsDisplayListBuilder* aDisplayListBuilder = nullptr);

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

  // These fields are used to save a copy of the display list for
  // empty transactions in layers-free mode.
  wr::BuiltDisplayList mBuiltDisplayList;
  nsTArray<WebRenderParentCommand> mParentCommands;

  // Those are data that we kept between transactions. We used to cache some
  // data in the layer. But in layers free mode, we don't have layer which
  // means we need some other place to cached the data between transaction.
  // That's what mItemData and mLastItemData do.
  //
  // In CreateOrRecycleWebRenderUserData function, we first check data is presence
  // in the mItemData, if not, check it is in mLastItemData. And in EndTransaction,
  // Replace mLastItemData with mItemData.
  nsRefPtrHashtable<FrameDisplayItemKeyHashEntry, WebRenderUserData> mItemData;
  nsRefPtrHashtable<FrameDisplayItemKeyHashEntry, WebRenderUserData> mLastItemData;

  // Layers that have been mutated. If we have an empty transaction
  // then a display item layer will no longer be valid
  // if it was a mutated layers.
  void AddMutatedLayer(Layer* aLayer);
  void ClearMutatedLayers();
  LayerRefArray mMutatedLayers;
  bool mTransactionIncomplete;

  bool mNeedsComposite;
  bool mIsFirstPaint;
  bool mEndTransactionWithoutLayers;
  FocusTarget mFocusTarget;

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
