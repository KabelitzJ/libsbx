using System.Runtime.InteropServices;
using Sbx.Core.Math;

namespace Sbx.Core
{

  /**
   * One instance of an InstancedMeshRenderer, exactly as the GPU reads it (scenes::instance_data,
   * 80 bytes): an affine transform relative to the renderer's node as the top three rows of a 4x4
   * matrix (row i = (m_i0, m_i1, m_i2, translation_i)), plus Color -- multiplied into the vertex
   * colour (white = unchanged) -- and CustomData, which only the material's own shader reads
   * (transform_data.custom_data). Build one with Create.
   */
  [StructLayout(LayoutKind.Sequential, Pack = 4)]
  public struct InstanceData
  {
    public Vector4 Row0;
    public Vector4 Row1;
    public Vector4 Row2;
    public Color Color;
    public Vector4 CustomData;

    /** Scaled, then rotated, then moved to position; Color white unless given. */
    public static InstanceData Create(Vector3 position, Quaternion rotation, Vector3 scale, Color? color = null, Vector4 customData = default)
    {
      float x = rotation.X, y = rotation.Y, z = rotation.Z, w = rotation.W;

      // Rotation matrix rows, each column scaled by the scale on that axis.
      return new InstanceData
      {
        Row0 = new Vector4((1 - 2 * (y * y + z * z)) * scale.X, 2 * (x * y - z * w) * scale.Y, 2 * (x * z + y * w) * scale.Z, position.X),
        Row1 = new Vector4(2 * (x * y + z * w) * scale.X, (1 - 2 * (x * x + z * z)) * scale.Y, 2 * (y * z - x * w) * scale.Z, position.Y),
        Row2 = new Vector4(2 * (x * z - y * w) * scale.X, 2 * (y * z + x * w) * scale.Y, (1 - 2 * (x * x + y * y)) * scale.Z, position.Z),
        Color = color ?? new Color(1.0f, 1.0f, 1.0f, 1.0f),
        CustomData = customData,
      };
    }

  } // struct InstanceData

} // namespace Sbx.Core
