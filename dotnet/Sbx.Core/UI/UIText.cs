using Sbx.Core;
using Sbx.Core.Math;

namespace Sbx.Core.UI
{

  /** Where a UIText's lines sit in its rect, per axis: Start is left/top. */
  public enum TextAlign : uint
  {
    Start,
    Center,
    End,
  }

  public class UIText : Component
  {

    public TextAlign HorizontalAlign
    {
      get { unsafe { uint horizontal = 0, vertical = 0; InternalCalls.UIText_GetAlignment(UUID, &horizontal, &vertical); return (TextAlign)horizontal; } }
      set => SetAlignment(value, VerticalAlign);
    }

    public TextAlign VerticalAlign
    {
      get { unsafe { uint horizontal = 0, vertical = 0; InternalCalls.UIText_GetAlignment(UUID, &horizontal, &vertical); return (TextAlign)vertical; } }
      set => SetAlignment(HorizontalAlign, value);
    }

    public void SetAlignment(TextAlign horizontal, TextAlign vertical)
    {
      unsafe { InternalCalls.UIText_SetAlignment(UUID, (uint)horizontal, (uint)vertical); }
    }

    public string? Text
    {
      get { unsafe { return InternalCalls.UIText_GetText(UUID); } }
      set { unsafe { InternalCalls.UIText_SetText(UUID, value); } }
    }

    public float FontSize
    {
      get { unsafe { float value; InternalCalls.UIText_GetFontSize(UUID, &value); return value; } }
      set { unsafe { InternalCalls.UIText_SetFontSize(UUID, value); } }
    }

    public Color Color
    {
      get { unsafe { Color value; InternalCalls.UIText_GetColor(UUID, &value); return value; } }
      set { unsafe { InternalCalls.UIText_SetColor(UUID, &value); } }
    }

    /** Assigns (or reassigns) which font asset this text renders with. path is project-relative, e.g. "fonts/Roboto-Regular.ttf". */
    public void LoadFont(string path)
    {
      unsafe { InternalCalls.UIText_LoadFont(UUID, path); }
    }

  } // class UIText

} // namespace Sbx.Core.UI
