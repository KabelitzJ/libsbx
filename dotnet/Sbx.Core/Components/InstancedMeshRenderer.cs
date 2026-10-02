using System;
using Sbx.Core;
using Sbx.Core.Math;

namespace Sbx.Core.Components
{

  /**
   * One mesh drawn many times in a single instanced draw (Godot's MultiMesh) -- trees, houses,
   * props: no node per object, nothing re-uploaded per frame, and every instance frustum-culled on
   * the GPU. Backed by the engine's instanced_mesh_renderer. The mesh (SetGeometry) and the
   * instances (SetInstances) are swapped in at the end of the frame, like MeshRenderer's geometry.
   * Opaque and alpha-masked materials only.
   */
  public class InstancedMeshRenderer : Component
  {

    /** The mesh every instance draws; same rules as MeshRenderer.SetGeometry (no tint: use the material, or each instance's Color). */
    public void SetGeometry(ReadOnlySpan<Vector3> positions, ReadOnlySpan<Vector3> normals, ReadOnlySpan<Vector2> uvs, ReadOnlySpan<uint> indices, ReadOnlySpan<Color> colors = default)
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
          InternalCalls.InstancedMeshRenderer_SetGeometry(UUID, positionsPtr, normalsPtr, uvsPtr, colorsPtr, (uint)positions.Length, indicesPtr, (uint)indices.Length);
        }
      }
    }

    public void SetMaterial(Material material)
    {
      unsafe { InternalCalls.InstancedMeshRenderer_SetMaterial(UUID, material.UUID); }
    }

    /** Replaces every instance (empty: draws nothing). */
    public void SetInstances(ReadOnlySpan<InstanceData> instances)
    {
      unsafe
      {
        fixed (InstanceData* ptr = instances)
        {
          InternalCalls.InstancedMeshRenderer_SetInstances(UUID, ptr, (uint)instances.Length);
        }
      }
    }

  } // class InstancedMeshRenderer

} // namespace Sbx.Core.Components
