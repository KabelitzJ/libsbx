namespace Sbx.Core
{

  public class ParticleEffect : Component
  {

    /** Assigns (or reassigns) which .particle_effect asset this component plays. path is project-relative, e.g. "particles/impact_spark.particle_effect". */
    public void Load(string path)
    {
      unsafe { InternalCalls.ParticleEffect_Load(UUID, path); }
    }

    public void Play()
    {
      unsafe { InternalCalls.ParticleEffect_Play(UUID); }
    }

    public void Pause()
    {
      unsafe { InternalCalls.ParticleEffect_Pause(UUID); }
    }

    public void Stop()
    {
      unsafe { InternalCalls.ParticleEffect_Stop(UUID); }
    }

    public bool Loop
    {
      get { unsafe { return InternalCalls.ParticleEffect_GetLoop(UUID); } }
      set { unsafe { InternalCalls.ParticleEffect_SetLoop(UUID, value); } }
    }

    public bool IsPlaying
    {
      get { unsafe { return InternalCalls.ParticleEffect_GetIsPlaying(UUID); } }
    }

  } // class ParticleEffect

} // namespace Sbx.Core
