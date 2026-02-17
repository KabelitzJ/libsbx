using Sbx.Core;
using Sbx.Math;

namespace Demo
{
  
  public class CharacterMovement : Behavior
  {
    
    public float MaxSpeed = 8f;
    public float GroundAcceleration = 40f;
    public float AirAcceleration = 10f;
    public float Gravity = -25f;
    public float JumpImpulse = 10f;
    public float GroundFriction = 12f;

    private Vector3 _velocity;
    private bool _isGrounded;

    private IInputProvider _input;
    private CharacterController _controller;
    private Transform _transform;

    public override void OnCreate()
    {
      _input = new KeyboardMouseInputProvider();
      _controller = GetComponent<CharacterController>();
      _transform = GetComponent<Transform>();
    }

    public override void OnUpdate()
    {
      var state = _input.GetState();

      UpdateMotor(state);
    }

    private void UpdateMotor(PlayerInputState state)
    {
      // UpdateGroundedState();

      var moveDir = GetCameraRelativeMove(state);

      ApplyMovement(moveDir);
      ApplyJump(state);
      ApplyGravity();
      ApplyFriction(moveDir);

      MoveAndCollide();
    }

    private Vector3 GetCameraRelativeMove(PlayerInputState state)
    {
      var forward = Camera.Main.FlatForward;
      var right   = Camera.Main.FlatRight;

      var move = forward * state.Move.Y + right * state.Move.X;

      move = move.Normalized();

      return move;
    }

    private void ApplyMovement(Vector3 desiredDir)
    {
      float accel = _isGrounded ? GroundAcceleration : AirAcceleration;

      _velocity += desiredDir * accel * Time.DeltaTime;

      var horizontal = new Vector3(_velocity.X, 0.0f, _velocity.Z);

      if (horizontal.Length() > MaxSpeed)
      {
        horizontal = horizontal.Normalized() * MaxSpeed;
        _velocity.X = horizontal.X;
        _velocity.Z = horizontal.Z;
      }
    }

    private void ApplyFriction(Vector3 desiredDir)
    {
      if (!_isGrounded)
      {
        return;
      }

      if (desiredDir.LengthSquared() > 0.001f)
      {
        return;
      }

      var horizontal = new Vector3(_velocity.X, 0.0f, _velocity.Z);

      float speed = horizontal.Length();

      if (speed <= 0.0f)
      {
        return;
      }

      float drop = speed * GroundFriction * Time.DeltaTime;

      float newSpeed = MathF.Max(speed - drop, 0.0f);

      horizontal *= newSpeed / speed;

      _velocity.X = horizontal.X;
      _velocity.Z = horizontal.Z;
    }

    private void ApplyJump(PlayerInputState state)
    {
      if (_isGrounded && state.Jump)
      {
        _velocity.Y = JumpImpulse;
      }
    }

    private void ApplyGravity()
    {
      _velocity.Y += Gravity * Time.DeltaTime;
    }

    private void MoveAndCollide()
    {
      var displacement = _velocity * Time.DeltaTime;
      _controller.Move(displacement);

      _isGrounded = _controller.IsGrounded;

      var flags = _controller.Flags;

      // Hit ground — stop falling
      if (flags.HasFlag(CollisionFlags.Below) && _velocity.Y < 0f)
      {
        _velocity.Y = 0f;
      }

      // Hit ceiling — stop rising
      if (flags.HasFlag(CollisionFlags.Above) && _velocity.Y > 0f)
      {
        _velocity.Y = 0f;
      }
    }


  } // class CharacterMotor

} // namespace Demo