using Sbx.Core;
using Sbx.Core.Math;

namespace Sbx.Core.UI
{

  /**
   * A tinted rectangle (needs a RectTransform on the same node to be placed). Native canvas_module
   * already draws a texture (canvas::ui_image::sprite) when one is assigned -- from script, use
   * LoadSprite; assigning one at author time is also already exposed in the editor's UI Image
   * inspector section, no scripting needed for that case.
   */
  public class UIImage : Component
  {

    public Color Tint
    {
      get { unsafe { Color value; InternalCalls.UIImage_GetTint(UUID, &value); return value; } }
      set { unsafe { InternalCalls.UIImage_SetTint(UUID, &value); } }
    }

    public void LoadSprite(string path)
    {
      unsafe { InternalCalls.UIImage_LoadSprite(UUID, path); }
    }

  } // class UIImage

} // namespace Sbx.Core.UI
