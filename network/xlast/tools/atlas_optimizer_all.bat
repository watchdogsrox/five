REM you can find atlas_optimizer in rage\tools\base\exes on head of rage
for %%f in (atlas_gta4ps3_v*.c) do (
p4 add optimised_%%f
p4 edit optimised_%%f
atlas_optimizer < %%f > optimised_%%f
)
