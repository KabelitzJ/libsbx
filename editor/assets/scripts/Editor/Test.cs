using Sbx.Core;
using Sbx.Math;

namespace Editor
{
  
  public class Test : Behavior
  {
    Transform transform;
    
    public override void OnCreate()
    {
      transform = GetComponent<Transform>();

      Logger.Info("Test script created for entity with UUID: {0}", UUID);
      Logger.Info("Transform component: {0}", transform.Position);
    }

    public override void OnUpdate()
    {

    }

  }

}
