using Sbx.Core;
using Sbx.Core.Math;

namespace Sbx.Core.Components
{

  public class Animator : Component
  {

    public bool Playing
    {
      get { unsafe { return InternalCalls.Animator_GetPlaying(UUID); } }
      set { unsafe { InternalCalls.Animator_SetPlaying(UUID, value); } }
    }

    /** Name of the state currently playing (or being crossfaded from, mid-transition) -- empty if no graph is assigned. */
    /** Whether the base layer's root joint motion moves this node (or its CharacterController) instead of the mesh drifting off it. */
    public bool ApplyRootMotion
    {
      get { unsafe { return InternalCalls.Animator_GetApplyRootMotion(UUID); } }
      set { unsafe { InternalCalls.Animator_SetApplyRootMotion(UUID, value); } }
    }

    /** This frame's root motion as a world-space displacement, ready for CharacterController.Move; read it in OnAnimatorMove. */
    public Vector3 DeltaPosition
    {
      get
      {
        var value = new Vector3();
        unsafe { InternalCalls.Animator_GetDeltaPosition(UUID, &value); }
        return value;
      }
    }

    /** This frame's root turn, to multiply onto the node's local rotation. */
    public Quaternion DeltaRotation
    {
      get
      {
        var value = Quaternion.Identity;
        unsafe { InternalCalls.Animator_GetDeltaRotation(UUID, &value); }
        return value;
      }
    }

    /** Moves an IK chain's goal (world space); the end joint reaches for it by the chain's position weight. Goals persist until changed. */
    public void SetIKPosition(string chain, Vector3 position)
    {
      unsafe { InternalCalls.Animator_SetIKPosition(UUID, chain, &position); }
    }

    /** Turns an IK chain's end joint toward a world-space rotation, by the chain's rotation weight. */
    public void SetIKRotation(string chain, Quaternion rotation)
    {
      unsafe { InternalCalls.Animator_SetIKRotation(UUID, chain, &rotation); }
    }

    /** Where the chain's middle joint (knee, elbow) should point, world space. */
    public void SetIKHint(string chain, Vector3 position)
    {
      unsafe { InternalCalls.Animator_SetIKHint(UUID, chain, &position); }
    }

    public void ClearIKHint(string chain)
    {
      unsafe { InternalCalls.Animator_ClearIKHint(UUID, chain); }
    }

    /** How far a chain follows its goal position and rotation (0 to 1 each); 0 leaves the animation (or foot placement) alone. */
    public void SetIKWeights(string chain, float positionWeight, float rotationWeight)
    {
      unsafe { InternalCalls.Animator_SetIKWeights(UUID, chain, positionWeight, rotationWeight); }
    }

    /** Turns the head (and some spine) toward a world-space point, by weight (0 to 1). */
    public void SetLookAt(Vector3 position, float weight)
    {
      unsafe { InternalCalls.Animator_SetLookAt(UUID, &position, weight); }
    }

    /** The base layer's current state. */
    public string? CurrentStateName
    {
      get { unsafe { return InternalCalls.Animator_GetCurrentStateName(UUID); } }
    }

    /** How many layers the graph has, the base layer included. */
    public int LayerCount
    {
      get { unsafe { return InternalCalls.Animator_GetLayerCount(UUID); } }
    }

    /** One layer's current state (0 is the base layer). */
    public string? GetCurrentStateName(int layer)
    {
      unsafe { return InternalCalls.Animator_GetLayerStateName(UUID, layer); }
    }

    /** A layer's index by name, -1 if the graph has none by that name. */
    public int GetLayerIndex(string name)
    {
      unsafe { return InternalCalls.Animator_GetLayerIndex(UUID, name); }
    }

    /** A layer's weight; always 1 for the base layer. */
    public float GetLayerWeight(int layer)
    {
      unsafe { return InternalCalls.Animator_GetLayerWeight(UUID, layer); }
    }

    /** Fades a layer in or out (0 to 1); the base layer always has full weight. */
    public void SetLayerWeight(int layer, float weight)
    {
      unsafe { InternalCalls.Animator_SetLayerWeight(UUID, layer, weight); }
    }

    public void SetLayerWeight(string layer, float weight)
    {
      var index = GetLayerIndex(layer);

      if (index >= 0)
      {
        SetLayerWeight(index, weight);
      }
    }

    public void SetFloat(string name, float value)
    {
      unsafe { InternalCalls.Animator_SetFloat(UUID, name, value); }
    }

    public void SetBool(string name, bool value)
    {
      unsafe { InternalCalls.Animator_SetBool(UUID, name, value); }
    }

    public void SetInt(string name, int value)
    {
      unsafe { InternalCalls.Animator_SetInt(UUID, name, value); }
    }

    /** Fires a trigger parameter -- consumed (reset) by the engine once this frame's transitions have been checked. */
    public void SetTrigger(string name)
    {
      unsafe { InternalCalls.Animator_SetTrigger(UUID, name); }
    }

    public float GetFloat(string name)
    {
      unsafe { return InternalCalls.Animator_GetFloat(UUID, name); }
    }

    public bool GetBool(string name)
    {
      unsafe { return InternalCalls.Animator_GetBool(UUID, name); }
    }

    public int GetInt(string name)
    {
      unsafe { return InternalCalls.Animator_GetInt(UUID, name); }
    }

  } // class Animator

} // namespace Sbx.Core.Components
