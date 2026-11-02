using System.Runtime.InteropServices;

using Sbx.Core;
using Sbx.Core.Math;

namespace Sbx.Core.Components
{

  // Mirrors interop.hpp's reflection_probe_component_data.
  [StructLayout(LayoutKind.Sequential)]
  internal struct ReflectionProbeData
  {
    public Vector3 BoxExtents;
    public float BlendDistance;
    public float Intensity;
    public int Importance;
    public uint BoxProjection;
  } // struct ReflectionProbeData

  /**
   * Surfaces inside its box reflect its baked capture instead of the sky. The box is centered on the node and turns with it, ignoring its scale.
   * Baking happens in the editor; these are the settings a script can change at runtime.
   */
  public class ReflectionProbe : Component
  {

    /** Half extents along the node's axes. */
    public Vector3 BoxExtents
    {
      get => Get().BoxExtents;
      set { var data = Get(); data.BoxExtents = value; Set(data); }
    }

    /** How far inside the box the probe fades in from its faces. */
    public float BlendDistance
    {
      get => Get().BlendDistance;
      set { var data = Get(); data.BlendDistance = value; Set(data); }
    }

    public float Intensity
    {
      get => Get().Intensity;
      set { var data = Get(); data.Intensity = value; Set(data); }
    }

    /** Where boxes overlap, higher wins; equal importance prefers the smaller box. */
    public int Importance
    {
      get => Get().Importance;
      set { var data = Get(); data.Importance = value; Set(data); }
    }

    /** Corrects reflections for the box's walls; off treats the capture as infinitely far. */
    public bool BoxProjection
    {
      get => Get().BoxProjection != 0;
      set { var data = Get(); data.BoxProjection = value ? 1u : 0u; Set(data); }
    }

    private ReflectionProbeData Get()
    {
      var data = new ReflectionProbeData();
      unsafe { InternalCalls.ReflectionProbe_Get(UUID, &data); }
      return data;
    }

    private void Set(ReflectionProbeData data)
    {
      unsafe { InternalCalls.ReflectionProbe_Set(UUID, &data); }
    }

  } // class ReflectionProbe

} // namespace Sbx.Core.Components
