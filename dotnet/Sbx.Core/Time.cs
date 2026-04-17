using System;
using Sbx.Math;

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
  } // class Camera

} // namespace Sbx.Core