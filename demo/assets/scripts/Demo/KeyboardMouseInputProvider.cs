using Sbx.Core;
using Sbx.Math;

namespace Demo
{
  
  public class KeyboardMouseInputProvider : IInputProvider
  {

    private float _mouseSensitivity = 0.15f;

    public PlayerInputState GetState()
    {
      var state = new PlayerInputState();

      state.Move = GetMoveAxis();
      state.Look = GetMouseDelta() * _mouseSensitivity;

      state.Jump   = Input.IsKeyDown(KeyCode.Space);
      state.Sprint = Input.IsKeyDown(KeyCode.LeftShift);

      state.PrimaryFire   = Input.IsMouseButtonDown(MouseButton.Left);
      state.SecondaryFire = Input.IsMouseButtonDown(MouseButton.Right);

      state.Ability1 = Input.IsKeyDown(KeyCode.Q);
      state.Ability2 = Input.IsKeyDown(KeyCode.E);

      return state;
    }

    private Vector2 GetMoveAxis()
    {
      float x = 0f;
      float y = 0f;

      if (Input.IsKeyDown(KeyCode.A)) x -= 1.0f;
      if (Input.IsKeyDown(KeyCode.D)) x += 1.0f;
      if (Input.IsKeyDown(KeyCode.S)) y -= 1.0f;
      if (Input.IsKeyDown(KeyCode.W)) y += 1.0f;

      var v = new Vector2(x, y);

      v = v.Normalized();

      return v;
    }

    private Vector2 GetMouseDelta()
    {
      return Input.ScrollDelta();
    }

  } // class KeyboardMouseInputProvider

} // namespace Demo