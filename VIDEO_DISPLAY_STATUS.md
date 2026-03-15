# Video Display Implementation Status

## Current State (Task 4 - In Progress)

### What's Working ✅

1. **Video Data Reception**
   - iPad successfully connects and sends encrypted video data over TCP port 7001
   - MirroringListener receives 3000+ video packets
   - Packet reception is logged every 30 frames

2. **Display Infrastructure**
   - `HubModel` has `videoFrame` property with `videoFrameChanged()` signal
   - `VideoFrameProvider` QML image provider is registered and working
   - `Dashboard.qml` displays video using `Image` component with live updates
   - Video preview panel shows connection status and updates in real-time

3. **Video Decoder Pipeline**
   - `VideoDecoder` (Media Foundation H.265 hardware decoder) is initialized
   - Decoder has frame callback that fires when frames are decoded
   - Frame callback now updates dashboard with decoded frame info
   - NALParser → H265Demuxer → VideoDecoder pipeline exists and is wired

### What's NOT Working ❌

1. **Video Decryption**
   - Encrypted video data from MirroringListener is NOT being decrypted
   - AES-128-CTR decryption keys from AirPlay pairing are NOT accessible to the video callback
   - `AirPlay2Host::decryptVideoPacket()` exists but is not being called

2. **Pipeline Connection**
   - Encrypted video data from MirroringListener is NOT fed to NALParser
   - The decryption → parsing → decoding flow is NOT connected
   - Video decoder is initialized but receives NO data

3. **Texture to QImage Conversion**
   - Decoded frames are D3D12 textures (GPU memory)
   - No GPU readback to convert D3D12 texture → CPU memory → QImage
   - Dashboard shows placeholder text instead of actual video

## Current Behavior

When iPad is connected and mirroring:
- Dashboard shows: "Decoding Video" placeholder with frame info
- Logs show: "Encrypted video frame received" every 30 frames
- NO actual video decoding happens (decoder receives no data)
- NO actual iPad screen content is displayed

## What Needs to Be Done

### Phase 1: Connect Decryption Pipeline (HIGH PRIORITY)

**Problem**: Encrypted video data needs to be decrypted before feeding to NALParser.

**Solution**:
1. Store encryption keys in a global or accessible location when AirPlay session starts
2. In MirroringListener callback, decrypt the payload using `AirPlay2Host::decryptVideoPacket()`
3. Feed decrypted data to `g_nalParser->feedPacket()`

**Files to modify**:
- `src/main.cpp` (lines 940-960) - MirroringListener callback
- `src/engine/AirPlay2Host.cpp` - Expose encryption keys from session

**Code changes needed**:
```cpp
// In main.cpp, MirroringListener callback:
g_mirroringListener->setVideoDataCallback([](const uint8_t* header, size_t headerSize,
                                              const uint8_t* payload, size_t payloadSize) {
    // 1. Get encryption keys from current AirPlay session
    auto keys = g_airplay->getCurrentSessionKeys();  // NEW METHOD NEEDED
    
    // 2. Decrypt payload in-place
    std::vector<uint8_t> decrypted(payload, payload + payloadSize);
    aura::AirPlay2Host::decryptVideoPacket(decrypted.data(), decrypted.size(), 
                                           keys.videoKey, keys.videoIV);
    
    // 3. Feed to NAL parser
    if (g_nalParser) {
        g_nalParser->feedPacket(std::span<const uint8_t>(decrypted.data(), decrypted.size()));
    }
});
```

### Phase 2: GPU Texture Readback (MEDIUM PRIORITY)

**Problem**: Decoded frames are D3D12 textures in GPU memory, but QImage needs CPU memory.

**Solution**:
1. Create a D3D12 readback buffer
2. Copy texture from GPU → CPU in VideoDecoder frame callback
3. Convert NV12 format → RGB888 for QImage
4. Update HubModel with actual frame data

**Files to modify**:
- `src/main.cpp` (lines 700-750) - VideoDecoder frame callback
- `src/display/DX12Manager.cpp` - Add texture readback helper

**Performance note**: GPU readback is expensive. Only do it every N frames (e.g., every 5th frame) for dashboard preview.

### Phase 3: Optimize Display (LOW PRIORITY)

**Optimizations**:
- Use QImage::Format_RGB888 for better performance
- Scale down large frames (e.g., 4K → 1080p) before display
- Use double buffering to prevent tearing
- Add frame rate limiting (30 FPS max for dashboard preview)

## Technical Details

### Video Data Flow (Current)

```
iPad → TCP 7001 → MirroringListener → [ENCRYPTED DATA] → ❌ STOPS HERE
```

### Video Data Flow (Target)

```
iPad → TCP 7001 → MirroringListener → Decrypt (AES-128-CTR) → 
NALParser → H265Demuxer → VideoDecoder (MF H.265) → 
D3D12 Texture → GPU Readback → QImage → HubModel → Dashboard.qml
```

### Encryption Details

- **Algorithm**: AES-128-CTR (Counter mode)
- **Key Source**: Derived during AirPlay SRP-6a pairing
- **Key Storage**: Currently in `AirPlaySessionState` (private to AirPlay2Host)
- **Decryption Function**: `AirPlay2Host::decryptVideoPacket()` (static method)

### Frame Format

- **Codec**: H.265/HEVC (High Efficiency Video Coding)
- **Container**: Annex B NAL units (Network Abstraction Layer)
- **Color Space**: NV12 (Y plane + interleaved UV plane)
- **Resolution**: Native device resolution (e.g., 2732×2048 for iPad Pro)

## Files Modified in This Session

1. `src/main.cpp` (lines 700-750, 940-960)
   - Added dashboard video preview to VideoDecoder frame callback
   - Simplified MirroringListener callback (removed test pattern)
   - Added TODO comments for decryption pipeline

2. `src/manager/HubModel.h` (line 35)
   - Already has `videoFrame` property (from previous session)

3. `src/manager/HubModel.cpp` (lines 90-93)
   - Already has `updateVideoFrame()` method (from previous session)

4. `src/manager/VideoFrameProvider.cpp`
   - Already implemented (from previous session)

5. `src/manager/qml/Dashboard.qml` (lines 200-250)
   - Already has video preview panel (from previous session)

## Next Steps

1. **Immediate**: Implement Phase 1 (decryption pipeline) to get actual video decoding working
2. **Short-term**: Implement Phase 2 (GPU readback) to display actual iPad screen content
3. **Long-term**: Implement Phase 3 (optimizations) for better performance

## Testing Checklist

- [ ] iPad connects successfully (already working)
- [ ] Encrypted video packets are received (already working)
- [ ] Video packets are decrypted (NOT IMPLEMENTED)
- [ ] Decrypted data is fed to NALParser (NOT IMPLEMENTED)
- [ ] VideoDecoder receives and decodes frames (NOT IMPLEMENTED)
- [ ] Dashboard shows "Decoding Video" placeholder (WORKING)
- [ ] Dashboard shows actual iPad screen content (NOT IMPLEMENTED)
- [ ] Video preview updates smoothly (NOT IMPLEMENTED)
- [ ] No memory leaks or crashes (NEEDS TESTING)

## Known Issues

1. **No video decryption**: Encrypted data is not being decrypted
2. **No pipeline connection**: Decrypted data is not fed to decoder
3. **No GPU readback**: Decoded textures are not converted to QImage
4. **Placeholder only**: Dashboard shows placeholder instead of actual video

## References

- AirPlay 2 Protocol: Uses AES-128-CTR for video encryption
- Media Foundation: Windows hardware video decoder API
- D3D12: DirectX 12 GPU texture format
- QImage: Qt image format for display in QML
