namespace Sbx.Core.Physics
{

  /**
   * A 32-bit set of layers, one bit per index -- matches the native scenes::layer_mask. A
   * blittable struct (a single uint field) so it marshals to/from a scripted field exactly like
   * Vector3 does, with no special-casing on the native side.
   */
  public struct LayerMask
  {

    public uint Value;

    public LayerMask(uint value)
    {
      Value = value;
    }

    public static LayerMask Everything => new LayerMask(0xFFFFFFFFu);

    public static LayerMask Nothing => new LayerMask(0u);

    public static LayerMask FromLayer(int layer) => new LayerMask(1u << layer);

    public readonly bool Test(int layer) => (Value & (1u << layer)) != 0u;

    public void Set(int layer) => Value |= (1u << layer);

    public void Clear(int layer) => Value &= ~(1u << layer);

  } // struct LayerMask
} // namespace Sbx.Core.Physics
