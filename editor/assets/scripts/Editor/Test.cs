using Sbx.Core;
using Sbx.Math;

namespace Editor
{
  
  public class Test : Behavior
  {
    
    public override void OnCreate()
    {
      Logger.Info("OnCreate: {}", Time.DeltaTime);
    }

    public override void OnUpdate()
    {
      Logger.Info("OnUpdate: {}", Time.DeltaTime);
    }

  }

}
