using System;
using Sbx.Core.Math;

namespace Sbx.Core
{

  public static class Time
  {

    public static float DeltaTime
    {
      get { 
        float deltaTime;
        unsafe { InternalCalls.Time_DeltaTime(&deltaTime); }
        return deltaTime;
      }
    }
    /** Seconds per FixedUpdate step (Project Settings > Physics > Fixed Timestep). */
    public static float FixedDeltaTime
    {
      get { unsafe { return InternalCalls.Time_FixedDeltaTime(); } }
    }

  } // class Time

} // namespace Sbx.Core