using Sbx.Core;

namespace Sbx.Core.UI
{

  /** Mirrors native canvas::render_mode -- keep the order in sync. */
  public enum CanvasRenderMode : uint
  {
    ScreenSpaceOverlay,
    ScreenSpaceCamera,
    WorldSpace,
  }

  /**
   * Marks a node as the root of a UI hierarchy -- every RectTransform child (recursively) resolves
   * against this canvas' own rect. Defaults to ScreenSpaceOverlay (the canvas fills the whole
   * window); set Mode to WorldSpace to place it at the node in 3D instead.
   */
  public class Canvas : Component
  {

    /** Higher draws on top and wins hit-testing ties -- see canvas_module::update(). */
    public int SortOrder
    {
      get { unsafe { int value; InternalCalls.Canvas_GetSortOrder(UUID, &value); return value; } }
      set { unsafe { InternalCalls.Canvas_SetSortOrder(UUID, value); } }
    }

    public CanvasRenderMode Mode
    {
      get { unsafe { uint value = 0; InternalCalls.Canvas_GetMode(UUID, &value); return (CanvasRenderMode)value; } }
      set { unsafe { InternalCalls.Canvas_SetMode(UUID, (uint)value); } }
    }

    /** WorldSpace only: always face the camera, ignoring the node's own rotation. */
    public bool Billboard
    {
      get { unsafe { return InternalCalls.Canvas_GetBillboard(UUID); } }
      set { unsafe { InternalCalls.Canvas_SetBillboard(UUID, value); } }
    }

    /** WorldSpace only: world units per canvas pixel. */
    public float WorldScale
    {
      get { unsafe { float value; InternalCalls.Canvas_GetWorldScale(UUID, &value); return value; } }
      set { unsafe { InternalCalls.Canvas_SetWorldScale(UUID, value); } }
    }

    /**
     * Whether the cursor is currently over any interactable UIButton, across every canvas in the
     * scene. Any world-picking code (a road tool) should check this before casting its own ray, so
     * a click on the UI never falls through to the world underneath it.
     */
    public static bool WantsPointerCapture()
    {
      unsafe { return InternalCalls.Canvas_WantsPointerCapture(); }
    }

  } // class Canvas

} // namespace Sbx.Core.UI
