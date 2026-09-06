namespace Sbx.Core
{

  /**
   * Marks a node as the root of a UI hierarchy -- every RectTransform child (recursively) resolves
   * against this canvas' own rect. v1 only implements screen-space-overlay (the canvas fills the
   * whole window) -- see the native canvas::canvas_module's own doc comment for what's still ahead.
   */
  public class Canvas : Component
  {

    /** Higher draws on top and wins hit-testing ties -- see canvas_module::update(). */
    public int SortOrder
    {
      get { unsafe { int value; InternalCalls.Canvas_GetSortOrder(UUID, &value); return value; } }
      set { unsafe { InternalCalls.Canvas_SetSortOrder(UUID, value); } }
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

} // namespace Sbx.Core
