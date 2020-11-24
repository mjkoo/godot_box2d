#include "box2d_physics_body.h"

#include "box2d_joints.h"
#include "box2d_world.h"

bool Box2DPhysicsBody::create_b2Body() {
	if (world_node && !body) {
		ERR_FAIL_COND_V(!world_node->world, false);

		// Create body
		bodyDef.position = gd_to_b2(get_transform().get_origin());
		bodyDef.angle = get_transform().get_rotation();

		body = world_node->world->CreateBody(&bodyDef);
		body->GetUserData().owner = this;

		//print_line("body created");

		// Create fixtures
		// TODO Does this actually ever do anything? I believe this is handled by ENTER/EXIT_TREE.
		//      If we add an enabled/disabled property that destroys/recreates b2Objects, then
		//      this callback is a MUST
		auto child = fixtures.front();
		while (child) {
			print_line("calling child fixture");
			child->get()->on_parent_created(this);
			child = child->next();
		}

		update_mass(false);

		// Notify joints
		auto joint = joints.front();
		while (joint) {
			joint->get()->on_parent_created(this);
			joint = joint->next();
		}

		set_physics_process_internal(true);
		return true;
	}
	return false;
}

bool Box2DPhysicsBody::destroy_b2Body() {
	if (body) {
		ERR_FAIL_COND_V(!world_node, false);
		ERR_FAIL_COND_V(!world_node->world, false);

		// Destroy body
		world_node->world->DestroyBody(body);
		//print_line("body destroyed");
		body = NULL;

		// b2Fixture destruction is handled by Box2D

		// b2Joint destruction is handled by Box2D

		set_physics_process_internal(false);
		return true;
	}
	return false;
}

void Box2DPhysicsBody::update_mass(bool p_calc_reset) {
	if (body) {
		if (use_custom_massdata) {
			body->SetMassData(&massDataDef);
		} else if (p_calc_reset) {
			body->ResetMassData();
		}
	}
}

void Box2DPhysicsBody::_notification(int p_what) {
	// TODO finalize implementation to imitate behavior from RigidBody2D and Kinematic (static too?)
	switch (p_what) {
		case NOTIFICATION_PREDELETE: {

			// Inform joints that this node is no more
			auto joint = joints.front();
			while (joint) {
				joint->get()->on_node_predelete(this);
				joint = joint->next();
			}

		} break;
		case NOTIFICATION_ENTER_TREE: {

			last_valid_xform = get_global_transform();

			// Find the Box2DWorld
			Node *_ancestor = get_parent();
			Box2DWorld *new_world = NULL;
			while (_ancestor && !new_world) {
				new_world = Object::cast_to<Box2DWorld>(_ancestor);
				_ancestor = _ancestor->get_parent();
			}

			// If new parent, recreate body
			if (new_world != world_node) {
				// Destroy b2Body
				if (world_node) {
					destroy_b2Body();
					if (world_node) {
						world_node->box2d_children.erase(this);
					}
				}
				world_node = new_world;
				// Create b2Body
				if (world_node) {
					world_node->box2d_children.insert(this);

					if (world_node->world) {
						create_b2Body();
					}
				}
			}

		} break;
		case NOTIFICATION_EXIT_TREE: {

			// Don't destroy body. It could be exiting/entering.
			// Body should be destroyed in destructor if node is being freed.

			// TODO What do we do if it exits the tree, the ref is kept (in a script), and it's never destroyed?
			//      Exiting w/o reentering should destroy body.
			//      This applies to Box2DFixture and Box2DJoint as well.

		} break;
		case NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {

			// Send new transform to physics
			Transform2D new_xform = get_transform();

			bodyDef.position = gd_to_b2(new_xform.get_origin());
			bodyDef.angle = new_xform.get_rotation();

			if (body) {
				body->SetTransform(gd_to_b2(new_xform.get_origin()), new_xform.get_rotation());
				// Revert changes. Node transform shall be updated on physics process.
				if (body->GetType() != b2_staticBody) {
					//set_notify_local_transform(false);
					//set_global_transform(last_valid_xform);
					//set_notify_local_transform(true);
				}
			}

		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {

			// TODO figure out if this can instead be a callback from Box2D.
			//		I don't think it can.
			if (body && body->IsAwake()) {
				set_block_transform_notify(true);
				set_transform(b2_to_gd(body->GetTransform()));
				set_block_transform_notify(false);

				// handle contact monitoring or something? (see RigidBody2D::_direct_state_changed)
			}

		} break;
	}
}

void Box2DPhysicsBody::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_linear_velocity", "linear_velocity"), &Box2DPhysicsBody::set_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_linear_velocity"), &Box2DPhysicsBody::get_linear_velocity);
	ClassDB::bind_method(D_METHOD("set_angular_velocity", "angular_velocity"), &Box2DPhysicsBody::set_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_angular_velocity"), &Box2DPhysicsBody::get_angular_velocity);
	ClassDB::bind_method(D_METHOD("set_use_custom_massdata", "use_custom_massdata"), &Box2DPhysicsBody::set_use_custom_massdata);
	ClassDB::bind_method(D_METHOD("get_use_custom_massdata"), &Box2DPhysicsBody::get_use_custom_massdata);
	ClassDB::bind_method(D_METHOD("set_mass", "mass"), &Box2DPhysicsBody::set_mass);
	ClassDB::bind_method(D_METHOD("get_mass"), &Box2DPhysicsBody::get_mass);
	ClassDB::bind_method(D_METHOD("set_inertia", "inertia"), &Box2DPhysicsBody::set_inertia);
	ClassDB::bind_method(D_METHOD("get_inertia"), &Box2DPhysicsBody::get_inertia);
	ClassDB::bind_method(D_METHOD("set_center_of_mass", "center_of_mass"), &Box2DPhysicsBody::set_center_of_mass);
	ClassDB::bind_method(D_METHOD("get_center_of_mass"), &Box2DPhysicsBody::get_center_of_mass);
	ClassDB::bind_method(D_METHOD("set_mass_data", "mass", "inertia", "center_of_mass"), &Box2DPhysicsBody::set_mass_data);
	ClassDB::bind_method(D_METHOD("set_linear_damping", "linear_damping"), &Box2DPhysicsBody::set_linear_damping);
	ClassDB::bind_method(D_METHOD("get_linear_damping"), &Box2DPhysicsBody::get_linear_damping);
	ClassDB::bind_method(D_METHOD("set_angular_damping", "angular_damping"), &Box2DPhysicsBody::set_angular_damping);
	ClassDB::bind_method(D_METHOD("get_angular_damping"), &Box2DPhysicsBody::get_angular_damping);
	ClassDB::bind_method(D_METHOD("set_gravity_scale", "gravity_scale"), &Box2DPhysicsBody::set_gravity_scale);
	ClassDB::bind_method(D_METHOD("get_gravity_scale"), &Box2DPhysicsBody::get_gravity_scale);
	ClassDB::bind_method(D_METHOD("set_type", "type"), &Box2DPhysicsBody::set_type);
	ClassDB::bind_method(D_METHOD("get_type"), &Box2DPhysicsBody::get_type);
	ClassDB::bind_method(D_METHOD("set_bullet", "bullet"), &Box2DPhysicsBody::set_bullet);
	ClassDB::bind_method(D_METHOD("is_bullet"), &Box2DPhysicsBody::is_bullet);
	ClassDB::bind_method(D_METHOD("set_awake", "awake"), &Box2DPhysicsBody::set_awake);
	ClassDB::bind_method(D_METHOD("is_awake"), &Box2DPhysicsBody::is_awake);
	ClassDB::bind_method(D_METHOD("set_can_sleep", "can_sleep"), &Box2DPhysicsBody::set_can_sleep);
	ClassDB::bind_method(D_METHOD("get_can_sleep"), &Box2DPhysicsBody::get_can_sleep);
	ClassDB::bind_method(D_METHOD("set_fixed_rotation", "fixed_rotation"), &Box2DPhysicsBody::set_fixed_rotation);
	ClassDB::bind_method(D_METHOD("is_fixed_rotation"), &Box2DPhysicsBody::is_fixed_rotation);

	ClassDB::bind_method(D_METHOD("apply_force", "force", "point"), &Box2DPhysicsBody::apply_force, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("apply_central_force", "force"), &Box2DPhysicsBody::apply_central_force, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("apply_torque", "torque"), &Box2DPhysicsBody::apply_torque, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("apply_linear_impulse", "impulse", "point"), &Box2DPhysicsBody::apply_linear_impulse, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("apply_central_linear_impulse", "impulse"), &Box2DPhysicsBody::apply_central_linear_impulse, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("apply_torque_impulse", "impulse"), &Box2DPhysicsBody::apply_torque_impulse, DEFVAL(true));

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "gravity_scale", PROPERTY_HINT_RANGE, "-128,128,0.01"), "set_gravity_scale", "get_gravity_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "type", PROPERTY_HINT_ENUM, "Static,Kinematic,Rigid"), "set_type", "get_type");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bullet"), "set_bullet", "is_bullet");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "awake"), "set_awake", "is_awake");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "can_sleep"), "set_can_sleep", "get_can_sleep");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fixed_rotation"), "set_fixed_rotation", "is_fixed_rotation");
	ADD_GROUP("Linear", "linear_");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "linear_velocity"), "set_linear_velocity", "get_linear_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "linear_damping", PROPERTY_HINT_RANGE, "-1,1,0.001"), "set_linear_damping", "get_linear_damping");
	ADD_GROUP("Angular", "angular_");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "angular_velocity"), "set_angular_velocity", "get_angular_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "angular_damping"), "set_angular_damping", "get_angular_damping");
	ADD_GROUP("Custom Mass Data", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_custom_massdata"), "set_use_custom_massdata", "get_use_custom_massdata");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "mass", PROPERTY_HINT_EXP_RANGE, "0.01,65535,0.01"), "set_mass", "get_mass");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "inertia", PROPERTY_HINT_EXP_RANGE, "0.01,65535,0.01"), "set_inertia", "get_inertia");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "center_of_mass"), "set_center_of_mass", "get_center_of_mass");

	BIND_ENUM_CONSTANT(MODE_RIGID);
	BIND_ENUM_CONSTANT(MODE_STATIC);
	BIND_ENUM_CONSTANT(MODE_KINEMATIC);
}

void Box2DPhysicsBody::on_parent_created(Node *) {
	//if (create_b2Body()) {
	//	print_line("body created");
	//}
	WARN_PRINT("BODY CREATED IN CALLBACK");
}

String Box2DPhysicsBody::get_configuration_warning() const {
	String warning = Node2D::get_configuration_warning();

	Node *_ancestor = get_parent();
	Box2DWorld *new_world = NULL;
	while (_ancestor && !new_world) {
		new_world = Object::cast_to<Box2DWorld>(_ancestor);
		_ancestor = _ancestor->get_parent();
	}

	if (!new_world) {
		if (warning != String()) {
			warning += "\n\n";
		}
		warning += TTR("Box2DPhysicsBody only serves to provide bodies to a Box2DWorld node. Please only use it under the hierarchy of Box2DWorld.");
	}

	if (fixtures.size() == 0) {
		if (warning != String()) {
			warning += "\n\n";
		}
		warning += TTR("This node has no fixture, so it can't collide or interact with other objects.\nConsider adding a Box2DFixture subtype as a child to define its shape.");
	}

	return warning;
}

void Box2DPhysicsBody::set_linear_velocity(const Vector2 &p_vel) {
	if (body) {
		body->SetLinearVelocity(gd_to_b2(p_vel));
	}
	bodyDef.linearVelocity = gd_to_b2(p_vel);
}

Vector2 Box2DPhysicsBody::get_linear_velocity() const {
	if (body) {
		return b2_to_gd(body->GetLinearVelocity());
	}
	return b2_to_gd(bodyDef.linearVelocity);
}

void Box2DPhysicsBody::set_angular_velocity(const real_t p_omega) {
	if (body) {
		body->SetAngularVelocity(p_omega);
	}
	bodyDef.angularVelocity = p_omega;
}

real_t Box2DPhysicsBody::get_angular_velocity() const {
	if (body)
		return body->GetAngularVelocity();
	return bodyDef.angularVelocity;
}

void Box2DPhysicsBody::set_use_custom_massdata(bool p_use_custom) {
	use_custom_massdata = p_use_custom;
	update_mass();
}

bool Box2DPhysicsBody::get_use_custom_massdata() const {
	return use_custom_massdata;
}

void Box2DPhysicsBody::set_mass(const real_t p_mass) {
	ERR_FAIL_COND_MSG(!body, "b2Body is null.");
	if (!use_custom_massdata)
		WARN_PRINT("Changing mass related data without setting use_custom_massdata=true has no effect on the body.");
	massDataDef.mass = p_mass;
	update_mass(false);
}

real_t Box2DPhysicsBody::get_mass() const {
	if (body)
		return body->GetMass();
	return massDataDef.mass;
}

void Box2DPhysicsBody::set_inertia(const real_t p_inertia) {
	ERR_FAIL_COND_MSG(!body, "b2Body is null.");
	if (!use_custom_massdata)
		WARN_PRINT("Changing mass related data without setting use_custom_massdata=true has no effect on the body.");
	massDataDef.I = p_inertia;
	update_mass(false);
}

real_t Box2DPhysicsBody::get_inertia() const {
	if (body)
		return body->GetInertia();
	return massDataDef.I;
}

void Box2DPhysicsBody::set_center_of_mass(const Vector2 &p_center) {
	ERR_FAIL_COND_MSG(!body, "b2Body is null.");
	if (!use_custom_massdata)
		WARN_PRINT("Changing mass related data without setting use_custom_massdata=true has no effect on the body.");
	massDataDef.center = gd_to_b2(p_center);
	update_mass(false);
}

Vector2 Box2DPhysicsBody::get_center_of_mass() const {
	if (body)
		return b2_to_gd(body->GetLocalCenter());
	return b2_to_gd(massDataDef.center);
}

void Box2DPhysicsBody::set_mass_data(const real_t p_mass, const real_t p_inertia, const Vector2 &p_center) {
	ERR_FAIL_COND_MSG(!body, "b2Body is null.");
	massDataDef.mass = p_mass;
	massDataDef.I = p_inertia;
	massDataDef.center = gd_to_b2(p_center);
	update_mass(false);
}

void Box2DPhysicsBody::set_linear_damping(real_t p_damping) {
	linear_damping = p_damping;
	if (p_damping < 0) {
		linear_damping = -1;
		p_damping = GLOBAL_GET("physics/2d/default_linear_damp");
	}
	if (body)
		body->SetLinearDamping(p_damping);
	bodyDef.linearDamping = p_damping;
}

real_t Box2DPhysicsBody::get_linear_damping() const {
	return linear_damping;
}

void Box2DPhysicsBody::set_angular_damping(real_t p_damping) {
	angular_damping = p_damping;
	if (p_damping < 0) {
		angular_damping = -1;
		p_damping = GLOBAL_GET("physics/2d/default_angular_damp");
	}
	if (body)
		body->SetAngularDamping(p_damping);
	bodyDef.angularDamping = p_damping;
}

real_t Box2DPhysicsBody::get_angular_damping() const {
	return angular_damping;
}

void Box2DPhysicsBody::set_gravity_scale(real_t p_scale) {
	if (body)
		body->SetGravityScale(p_scale);
	bodyDef.gravityScale = p_scale;
}

real_t Box2DPhysicsBody::get_gravity_scale() const {
	return bodyDef.gravityScale;
}

void Box2DPhysicsBody::set_type(Mode p_type) {
	if (body)
		body->SetType(static_cast<b2BodyType>(p_type));
	bodyDef.type = static_cast<b2BodyType>(p_type);
}

Box2DPhysicsBody::Mode Box2DPhysicsBody::get_type() const {
	return static_cast<Mode>(bodyDef.type);
}

void Box2DPhysicsBody::set_bullet(bool p_ccd) {
	if (body)
		body->SetBullet(p_ccd);
	bodyDef.bullet = p_ccd;
}

bool Box2DPhysicsBody::is_bullet() const {
	return bodyDef.bullet;
}

void Box2DPhysicsBody::set_awake(bool p_awake) {
	if (body)
		body->SetAwake(p_awake);
	bodyDef.awake = p_awake;
}

bool Box2DPhysicsBody::is_awake() const {
	if (body)
		return body->IsAwake();
	return bodyDef.awake;
}

void Box2DPhysicsBody::set_can_sleep(bool p_can_sleep) {
	if (body)
		body->SetSleepingAllowed(p_can_sleep);
	bodyDef.allowSleep = p_can_sleep;
}

bool Box2DPhysicsBody::get_can_sleep() const {
	return bodyDef.allowSleep;
}

void Box2DPhysicsBody::set_fixed_rotation(bool p_fixed) {
	if (body)
		body->SetFixedRotation(p_fixed);
	bodyDef.fixedRotation = p_fixed;
}

bool Box2DPhysicsBody::is_fixed_rotation() const {
	return bodyDef.fixedRotation;
}

void Box2DPhysicsBody::apply_force(const Vector2 &force, const Vector2 &point, bool wake) {
	ERR_FAIL_COND_MSG(!body, "b2Body is null.");
	body->ApplyForce(gd_to_b2(force), gd_to_b2(point), wake);
}

void Box2DPhysicsBody::apply_central_force(const Vector2 &force, bool wake) {
	ERR_FAIL_COND_MSG(!body, "b2Body is null.");
	body->ApplyForceToCenter(gd_to_b2(force), wake);
}

void Box2DPhysicsBody::apply_torque(real_t torque, bool wake) {
	ERR_FAIL_COND_MSG(!body, "b2Body is null.");
	body->ApplyTorque(torque, wake);
}

void Box2DPhysicsBody::apply_linear_impulse(const Vector2 &impulse, const Vector2 &point, bool wake) {
	ERR_FAIL_COND_MSG(!body, "b2Body is null.");
	body->ApplyLinearImpulse(gd_to_b2(impulse), gd_to_b2(point), wake);
}

void Box2DPhysicsBody::apply_central_linear_impulse(const Vector2 &impulse, bool wake) {
	ERR_FAIL_COND_MSG(!body, "b2Body is null.");
	body->ApplyLinearImpulseToCenter(gd_to_b2(impulse), wake);
}

void Box2DPhysicsBody::apply_torque_impulse(real_t impulse, bool wake) {
	ERR_FAIL_COND_MSG(!body, "b2Body is null.");
	body->ApplyAngularImpulse(impulse, wake);
}

Box2DPhysicsBody::Box2DPhysicsBody() :
		massDataDef{ 1.0f, b2Vec2_zero, 0.5f }, // default for a disk of 1kg, 1m radius
		use_custom_massdata(false),
		linear_damping(0.0f),
		angular_damping(0.0f),
		world_node(NULL) {
	set_notify_local_transform(true);
}

Box2DPhysicsBody::~Box2DPhysicsBody() {
	if (body && world_node) {
		destroy_b2Body();
	} // else Box2D has/will clean up body
}
