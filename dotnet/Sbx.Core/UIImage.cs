using Sbx.Math;

namespace Sbx.Core
{

  /** A solid-color filled rectangle (needs a RectTransform on the same node to be placed). Texture support (tinted image, not just a flat color) is future work -- v1 only ever draws Tint. */
  public class UIImage : Component
  {

    public Color Tint
    {
      get { unsafe { Color value; InternalCalls.UIImage_GetTint(UUID, &value); return value; } }
      set { unsafe { InternalCalls.UIImage_SetTint(UUID, &value); } }
    }

  } // class UIImage

} // namespace Sbx.Core
