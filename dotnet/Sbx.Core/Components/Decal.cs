using System.Runtime.InteropServices;

using Sbx.Core;
using Sbx.Core.Math;

namespace Sbx.Core.Components
{

  // Mirrors interop.hpp's decal_component_data.
  [StructLayout(LayoutKind.Sequential)]
  internal struct DecalData
  {
    public ulong Albedo;
    public ulong Normal;
    public Color Color;
    public float Opacity;
    public float NormalStrength;
    public float AngleFadeStart;
    public float AngleFadeEnd;
    public float EdgeFade;
    public int SortOrder;
  } // struct DecalData

  /**
   * Projects textures onto the opaque surfaces inside its box: the unit cube under the node's transform, projecting along the node's -Z.
   * Scale the node to size the box. Transparent surfaces don't receive decals.
   */
  public class Decal : Component
  {

    /** sRGB albedo; its alpha masks the decal. Null projects Color alone. */
    public Texture2D? Albedo
    {
      get { var data = Get(); return data.Albedo != 0 ? new Texture2D(data.Albedo) : null; }
      set { var data = Get(); data.Albedo = value?.UUID ?? 0; Set(data); }
    }

    /** Tangent-space normal map in the decal's X/Y; load it with TextureFormat.Rgba8Unorm. */
    public Texture2D? Normal
    {
      get { var data = Get(); return data.Normal != 0 ? new Texture2D(data.Normal) : null; }
      set { var data = Get(); data.Normal = value?.UUID ?? 0; Set(data); }
    }

    /** Multiplies the albedo. */
    public Color Color
    {
      get => Get().Color;
      set { var data = Get(); data.Color = value; Set(data); }
    }

    public float Opacity
    {
      get => Get().Opacity;
      set { var data = Get(); data.Opacity = value; Set(data); }
    }

    public float NormalStrength
    {
      get => Get().NormalStrength;
      set { var data = Get(); data.NormalStrength = value; Set(data); }
    }

    /** Radians between the surface normal and the decal's +Z where the decal starts fading. */
    public float AngleFadeStart
    {
      get => Get().AngleFadeStart;
      set { var data = Get(); data.AngleFadeStart = value; Set(data); }
    }

    /** Radians where the decal is gone. */
    public float AngleFadeEnd
    {
      get => Get().AngleFadeEnd;
      set { var data = Get(); data.AngleFadeEnd = value; Set(data); }
    }

    /** Fraction of the box depth faded out at its near and far faces. */
    public float EdgeFade
    {
      get => Get().EdgeFade;
      set { var data = Get(); data.EdgeFade = value; Set(data); }
    }

    /** Higher draws over lower. */
    public int SortOrder
    {
      get => Get().SortOrder;
      set { var data = Get(); data.SortOrder = value; Set(data); }
    }

    private DecalData Get()
    {
      var data = new DecalData();
      unsafe { InternalCalls.Decal_Get(UUID, &data); }
      return data;
    }

    private void Set(DecalData data)
    {
      unsafe { InternalCalls.Decal_Set(UUID, &data); }
    }

  } // class Decal

} // namespace Sbx.Core.Components
