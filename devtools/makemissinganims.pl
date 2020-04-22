#! perl

# for the tf animations, copy appropriate guesses for missing ones, to aid development before
# all the anims are created.

$did_something=1;

while( $did_something )
{
  $did_something=0;
  &checkanim("swim.smd","");
  &checkanim("body_poses_lean.smd","");
  &checkanim("crouchwalke.smd","primary_crouch_walkn.smd");
  &checkanim("crouchwalkn.smd","primary_crouch_walkn.smd");
  &checkanim("crouchwalkne.smd","primary_crouch_walkn.smd");
  &checkanim("crouchwalknw.smd","primary_crouch_walkn.smd");
  &checkanim("crouchwalks.smd","primary_crouch_walkn.smd");
  &checkanim("crouchwalkse.smd","primary_crouch_walkn.smd");
  &checkanim("crouchwalksw.smd","primary_crouch_walkn.smd");
  &checkanim("crouchwalkw.smd","primary_crouch_walkn.smd");
  &checkanim("Melee_aim_matrix.smd","");
  &checkanim("melee_crouch_aim_matrix.smd","primary_crouch_aim_matrix.smd");
  &checkanim("melee_crouch_idle.smd","melee_idle.smd");
  &checkanim("melee_crouch_swing.smd","melee_swing.smd");
  &checkanim("melee_crouch_walkn.smd","crouchwalkn.smd");
  &checkanim("Melee_grenade_throw.smd","");
  &checkanim("melee_idle.smd","");
  &checkanim("Melee_jump.smd","");
  &checkanim("Melee_runN.smd","");
  &checkanim("Melee_runN_aim_matrix.smd","");
  &checkanim("melee_swim.smd","swim.smd");
  &checkanim("melee_swim_swing.smd","swim.smd");
  &checkanim("Melee_swing.smd","swim.smd");
  &checkanim("melee_walkn.smd","walkn.smd");
  &checkanim("Primary_aim_matrix.smd","");
  &checkanim("Primary_crouch_aim_matrix.smd","");
  &checkanim("Primary_crouch_fire.smd","");
  &checkanim("primary_crouch_idle.smd","");
  &checkanim("Primary_crouch_reload.smd","");
  &checkanim("Primary_crouch_walkN.smd","");
  &checkanim("Primary_fire.smd","");
  &checkanim("Primary_grenade_throw.smd","");
  &checkanim("primary_idle.smd","");
  &checkanim("Primary_jump.smd","");
  &checkanim("Primary_reload.smd","");
  &checkanim("Primary_runN.smd","");
  &checkanim("Primary_runN_aim_matrix.smd","");
  &checkanim("Primary_swim.smd","swim.smd");
  &checkanim("primary_swim_fire.smd","primary_fire.smd");
  &checkanim("primary_swim_fire.smd","swim.smd");
  &checkanim("primary_swim_reload.smd","swim.smd");
  &checkanim("primary_walkn.smd","walkn.smd");
  &checkanim("RunN.smd","");
  &checkanim("RunE.smd","runn.smd");
  &checkanim("RunNE.smd","runn.smd");
  &checkanim("RunNW.smd","runn.smd");
  &checkanim("RunS.smd","runn.cmd");
  &checkanim("RunSE.smd","runs.cmd");
  &checkanim("RunSW.smd","runs.cmd");
  &checkanim("RunW.smd","runn.smd");
  &checkanim("secondary_aim_matrix.smd","");
  &checkanim("secondary_crouch_aim_matrix.smd","");
  &checkanim("secondary_crouch_fire.smd","");
  &checkanim("secondary_crouch_idle.smd","");
  &checkanim("secondary_crouch_reload.smd","");
  &checkanim("secondary_crouch_walkn.smd","");
  &checkanim("secondary_fire.smd","");
  &checkanim("secondary_grenade_throw.smd","");
  &checkanim("secondary_idle.smd","");
  &checkanim("secondary_jump.smd","");
  &checkanim("secondary_reload.smd","");
  &checkanim("secondary_runn.smd","");
  &checkanim("secondary_runn_aim_matrix.smd","");
  &checkanim("secondary_swim.smd","swim.smd");
  &checkanim("secondary_swim_fire.smd","swim.smd");
  &checkanim("secondary_swim_reload.smd","swim.smd");
  &checkanim("secondary_walkn.smd","walkn.smd");
  &checkanim("walke.smd","rune.smd");
  &checkanim("walkn.smd","runn.smd");
  &checkanim("walkne.smd","runne.smd");
  &checkanim("walknw.smd","runnw.smd");
  &checkanim("walks.smd","runs.smd");
  &checkanim("walkse.smd","runse.smd");
  &checkanim("walksw.smd","runsw.smd");
  &checkanim("walkw.smd","runw.smd");
}


sub checkanim
  {
	local($desired_file,$fallback_file)=@_;
	unless( -e $desired_file)
	  {
		if (length($fallback_file) && (-e $fallback_file))
		  {
			print "**copying $fallback_file to $desired_file\n";
			`copy $fallback_file $desired_file`;
			$did_something=1;
		  }
		else
		  {
			print "no fallback present for missing $desired_file, fallback=$fallback_file\n";
		  }
	  }
	
  }
