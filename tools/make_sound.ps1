# Every sound the game ships, through one door.
#
# The audio is synthesised in two scripts - make_audio for what happens in the
# water, make_audio_surface for the rig, the bell, the reels and the cutscene
# stings - and for a long time there was nothing that ran both. That is the only
# reason half the game's audio looked orphaned: the sources were there, they
# just had no orchestrator the way the art has make_3d.
#
# Both are deterministic: same seed, same samples, byte-identical output. So
# this is safe to run at any time, and a wav that changes after running it means
# a synth actually changed, which is exactly what you want a regeneration to
# tell you.
$ErrorActionPreference = 'Stop'

foreach ($script in @('make_audio.ps1', 'make_audio_surface.ps1')) {
    "--- $script"
    & "$PSScriptRoot\$script"
}

"all sound written"
