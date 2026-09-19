namespace Sbx.Core
{

  /** Global rendering toggles (see the native scene_renderer_module). Not a Component -- these apply to the whole scene, not one node. */
  public static class RenderSettings
  {

    /** Draws opaque geometry as wireframe instead of filled triangles. Off by default. */
    public static bool WireframeEnabled
    {
      get { unsafe { return InternalCalls.RenderSettings_GetWireframeEnabled(); } }
      set { unsafe { InternalCalls.RenderSettings_SetWireframeEnabled(value); } }
    }

  } // class RenderSettings

} // namespace Sbx.Core
