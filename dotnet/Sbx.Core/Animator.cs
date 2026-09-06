namespace Sbx.Core
{

  public class Animator : Component
  {

    public bool Playing
    {
      get { unsafe { return InternalCalls.Animator_GetPlaying(UUID); } }
      set { unsafe { InternalCalls.Animator_SetPlaying(UUID, value); } }
    }

    /** Name of the state currently playing (or being crossfaded from, mid-transition) -- empty if no graph is assigned. */
    public string? CurrentStateName
    {
      get { unsafe { return InternalCalls.Animator_GetCurrentStateName(UUID); } }
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

} // namespace Sbx.Core
