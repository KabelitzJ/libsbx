using Sbx.Core;
using Sbx.Math;

namespace Demo
{

  public class Helmet : Behavior
  {

    private LayerMask selectionMask;
    private LayerMask targetMask;

    public void SayHello()
    {
      var tag = GetComponent<Tag>();
      var transform = GetComponent<Transform>();

      Logger.Info("Hello {0}", HasComponent<Tag>());
      Logger.Info("Hello {0}", Node);

      Logger.Info("Hello from {0}", tag);
      Logger.Info("Position: {0}", transform.Position);

      transform.Position = new Vector3(4, 5, 6);
    }

    public void SetTag(string value)
    {
      var tag = GetComponent<Tag>();

      tag.Value = value;
    }
    
    public override void OnUpdate()
    {
      if (Input.IsKeyPressed(KeyCode.Space))
      {
        Logger.Info("Space pressed from {0}", GetComponent<Tag>());
      }
    }

  } // class Helmet

} // namespace Demo

