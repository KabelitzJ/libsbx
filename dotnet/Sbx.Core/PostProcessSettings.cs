using System.Runtime.InteropServices;
using Sbx.Core.Math;

namespace Sbx.Core
{
  public enum DepthOfFieldMode : uint
  {
    /** Sharp around FocusDistance from the camera. */
    Distance = 0,
    /** Sharp in a horizontal band of the screen (tilt-shift), independent of depth. */
    ScreenBand = 1,
  }

  /**
   * A camera's post processing (CameraSettings.PostProcess) -- everything the renderer does to its
   * lit image before the UI. A copy: read it, change fields, assign it back. Every effect is off or
   * neutral by default; see scenes::post_process_settings for what each one does.
   *
   * Mirrors scripting::post_process_data field for field (bools and enums are uints there, the
   * lookup table a texture uuid -- hence the private backing fields).
   */
  [StructLayout(LayoutKind.Sequential)]
  public struct PostProcessSettings
  {
    /** EV stops, applied as exp2(Exposure) before tonemapping; 0 = unchanged. */
    public float Exposure;

    private uint _bloomEnabled;
    public float BloomIntensity;
    public float BloomThreshold;
    public float BloomKnee;

    private uint _depthOfFieldEnabled;
    private uint _depthOfFieldMode;
    /** Distance mode: world units from the camera where it's sharpest. */
    public float FocusDistance;
    /** Distance mode: depth of the sharp zone (world units); the blur ramps in over as much again. */
    public float FocusRange;
    /** Screen band mode: the sharp band's centre, as a fraction of the screen height from the top. */
    public float BandCenter;
    /** Screen band mode: the sharp band's height (fraction of the screen); the blur ramps in over as much again. */
    public float BandHeight;
    /** Full blur radius, as a fraction of the screen height. */
    public float MaxBlur;
    /** Taps per pixel (1..64): quality vs cost. */
    public uint BlurSamples;

    private ulong _lut;
    public float LutContribution;
    public float Contrast;
    public float Saturation;

    private uint _fogEnabled;
    /** Linear. */
    public Color FogColor;
    /** Per world unit, at FogBaseHeight. */
    public float FogDensity;
    /** World units from the camera before the fog begins. */
    public float FogStart;
    /** Thins by e per 1 / FogHeightFalloff units above FogBaseHeight; 0 = the same at every height. */
    public float FogHeightFalloff;
    public float FogBaseHeight;
    public float FogMaxOpacity;
    private uint _fogAffectsSky;

    private uint _ambientOcclusionEnabled;
    /** World units: how far away geometry still darkens a point. */
    public float AmbientOcclusionRadius;
    public float AmbientOcclusionIntensity;
    /** Hemisphere samples per (half-resolution) pixel, 1..32. */
    public uint AmbientOcclusionSamples;

    public bool AmbientOcclusionEnabled
    {
      get => _ambientOcclusionEnabled != 0;
      set => _ambientOcclusionEnabled = value ? 1u : 0u;
    }

    public bool FogEnabled
    {
      get => _fogEnabled != 0;
      set => _fogEnabled = value ? 1u : 0u;
    }

    public bool FogAffectsSky
    {
      get => _fogAffectsSky != 0;
      set => _fogAffectsSky = value ? 1u : 0u;
    }

    public bool BloomEnabled
    {
      get => _bloomEnabled != 0;
      set => _bloomEnabled = value ? 1u : 0u;
    }

    public bool DepthOfFieldEnabled
    {
      get => _depthOfFieldEnabled != 0;
      set => _depthOfFieldEnabled = value ? 1u : 0u;
    }

    public DepthOfFieldMode DepthOfFieldMode
    {
      get => (DepthOfFieldMode)_depthOfFieldMode;
      set => _depthOfFieldMode = (uint)value;
    }

    /** Colour grading lookup table (a size*size x size strip), or null for none. Always used unorm, however it was loaded. */
    public Texture2D? Lut
    {
      get => _lut != 0 ? Texture2D.FromHandle(_lut) as Texture2D : null;
      set => _lut = value?.UUID ?? 0;
    }
  } // struct PostProcessSettings
} // namespace Sbx.Core
