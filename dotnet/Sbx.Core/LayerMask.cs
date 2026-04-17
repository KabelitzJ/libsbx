using System;

namespace Sbx.Core
{

  public class LayerMask
  {

    private uint value;

    LayerMask(uint value)
    {
      this.value = value;
    }

    public void Set(uint layer)
    {
      value |= layer;
    }

    public void Clear(uint layer)
    {
      value &= ~layer;
    }

    public bool Test(uint layer)
    {
      return (value & layer) != 0;
    }

  } // class LayerMask
} // namespace Sbx.Core