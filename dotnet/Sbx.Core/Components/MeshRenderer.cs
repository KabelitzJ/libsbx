using System;
using Sbx.Core;
using Sbx.Core.Math;

namespace Sbx.Core.Components
{

  /**
   * Draws a mesh built at runtime from raw vertex/index data (see SetGeometry) -- backed by the
   * engine's ordinary mesh_renderer component and rendered through the normal opaque_pass path
   * (full lighting/shadows), the same as any glTF-imported model. Unlike Unity, there's no separate
   * Mesh object with its own lifetime to manage: geometry is owned entirely by this component.
   */
  public class MeshRenderer : Component
  {

    /**
     * Builds a mesh from raw vertex/index data and assigns it to this node, creating the underlying
     * mesh_renderer component (and a backing material) the first time this is called. Safe to call
     * every frame for live-edited geometry -- e.g. a road network's ghost preview while dragging --
     * without exhausting the engine's material capacity (see the native mesh_renderer_set_geometry's
     * own doc comment for why).
     *
     * positions/normals/uvs must all have the same length; indices must be a multiple of 3
     * (triangle list, matching the engine's counter_clockwise front-face convention as seen from
     * the side the normal points toward). tint defaults to white on first creation; passing it again
     * on a later call re-tints the existing material in place. colors is optional per-vertex color
     * (same length as positions when given) -- multiplied into the shaded result alongside tint, so
     * a single mesh can have differently-colored triangles (e.g. a hex grid where each cell is its
     * own color) instead of one uniform tint for the whole mesh. Leave it default to skip -- every
     * vertex stays opaque white.
     */
    public void SetGeometry(ReadOnlySpan<Vector3> positions, ReadOnlySpan<Vector3> normals, ReadOnlySpan<Vector2> uvs, ReadOnlySpan<uint> indices, Color? tint = null, ReadOnlySpan<Color> colors = default)
    {
      if (positions.Length != normals.Length || positions.Length != uvs.Length)
      {
        throw new ArgumentException("positions, normals and uvs must all have the same length");
      }

      if (!colors.IsEmpty && colors.Length != positions.Length)
      {
        throw new ArgumentException("colors, when provided, must have the same length as positions");
      }

      unsafe
      {
        fixed (Vector3* positionsPtr = positions)
        fixed (Vector3* normalsPtr = normals)
        fixed (Vector2* uvsPtr = uvs)
        fixed (Color* colorsPtr = colors)
        fixed (uint* indicesPtr = indices)
        {
          if (tint.HasValue)
          {
            var tintValue = tint.Value;
            InternalCalls.MeshRenderer_SetGeometry(UUID, positionsPtr, normalsPtr, uvsPtr, colorsPtr, (uint)positions.Length, indicesPtr, (uint)indices.Length, &tintValue);
          }
          else
          {
            InternalCalls.MeshRenderer_SetGeometry(UUID, positionsPtr, normalsPtr, uvsPtr, colorsPtr, (uint)positions.Length, indicesPtr, (uint)indices.Length, null);
          }
        }
      }
    }

  } // class MeshRenderer

} // namespace Sbx.Core.Components
