using Sbx.Math;

namespace Sbx.Core
{

  public class Rigidbody : Component
  {

    public Vector3 LinearVelocity
    {
      get
      {
        Vector3 velocity;
        unsafe { InternalCalls.Rigidbody_GetLinearVelocity(UUID, &velocity); }
        return velocity;
      }
      set
      {
        unsafe { InternalCalls.Rigidbody_SetLinearVelocity(UUID, &value); }
      }
    }

    public Vector3 AngularVelocity
    {
      get
      {
        Vector3 velocity;
        unsafe { InternalCalls.Rigidbody_GetAngularVelocity(UUID, &velocity); }
        return velocity;
      }
      set
      {
        unsafe { InternalCalls.Rigidbody_SetAngularVelocity(UUID, &value); }
      }
    }

    /** 0 reads back as infinite/immovable (a static or kinematic body) -- matches the engine's own rigidbody::inverse_mass convention. */
    public float Mass
    {
      get
      {
        float mass;
        unsafe { InternalCalls.Rigidbody_GetMass(UUID, &mass); }
        return mass;
      }
      set
      {
        unsafe { InternalCalls.Rigidbody_SetMass(UUID, value); }
      }
    }

    public float GravityScale
    {
      get
      {
        float scale;
        unsafe { InternalCalls.Rigidbody_GetGravityScale(UUID, &scale); }
        return scale;
      }
      set
      {
        unsafe { InternalCalls.Rigidbody_SetGravityScale(UUID, value); }
      }
    }

    /** Accumulates into this step's force_accumulator -- cleared by the solver after every fixed_update, so call this every FixedUpdate you want the force applied on. */
    public void AddForce(Vector3 force)
    {
      unsafe { InternalCalls.Rigidbody_AddForce(UUID, &force); }
    }

    /** Same accumulate-then-clear-each-step semantics as AddForce. */
    public void AddTorque(Vector3 torque)
    {
      unsafe { InternalCalls.Rigidbody_AddTorque(UUID, &torque); }
    }

  } // class Rigidbody

} // namespace Sbx.Core
