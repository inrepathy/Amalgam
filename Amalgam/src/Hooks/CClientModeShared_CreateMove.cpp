#include "../SDK/SDK.h"
#include "../Features/Aimbot/Aimbot.h"
#include "../Features/Backtrack/Backtrack.h"
#include "../Features/CritHack/CritHack.h"
#include "../Features/EnginePrediction/EnginePrediction.h"
#include "../Features/Misc/Misc.h"
#include "../Features/NoSpread/NoSpread.h"
#include "../Features/PacketManip/PacketManip.h"
#include "../Features/Resolver/Resolver.h"
#include "../Features/TickHandler/TickHandler.h"
#include "../Features/Visuals/Visuals.h"
#include "../Features/Visuals/FakeAngle/FakeAngle.h"

MAKE_HOOK(CClientModeShared_CreateMove, U::Memory.GetVFunc(I::ClientModeShared, 21), bool, __fastcall,
	CClientModeShared* rcx, float flInputSampleTime, CUserCmd* pCmd)
{
	G::Buttons = pCmd ? pCmd->buttons : G::Buttons;

	const bool bReturn = CALL_ORIGINAL(rcx, flInputSampleTime, pCmd);
	if (!pCmd || !pCmd->command_number)
		return bReturn;

	bool* pSendPacket = reinterpret_cast<bool*>(uintptr_t(_AddressOfReturnAddress()) + 0x128);

	//bool bOldAttacking = G::IsAttacking; // removeme
	G::PSilentAngles = G::SilentAngles = G::IsAttacking = G::IsThrowing = false;
	G::LastUserCmd = G::CurrentUserCmd ? G::CurrentUserCmd : pCmd;
	G::CurrentUserCmd = pCmd;

	I::Prediction->Update(I::ClientState->m_nDeltaTick, I::ClientState->m_nDeltaTick > 0, I::ClientState->last_command_ack, I::ClientState->lastoutgoingcommand + I::ClientState->chokedcommands);

	// correct tick_count for fakeinterp / nointerp
	pCmd->tick_count += TICKS_TO_TIME(F::Backtrack.flFakeInterp) - (Vars::Visuals::Removals::Interpolation.Value ? 0 : TICKS_TO_TIME(G::Lerp));
	if (G::Buttons & IN_DUCK) // lol
		pCmd->buttons |= IN_DUCK;

	auto pLocal = H::Entities.GetLocal();
	auto pWeapon = H::Entities.GetWeapon();
	if (pLocal && pWeapon)
	{	// Update Global Info
		{
			static int iStaticItemDefinitionIndex = 0;
			int iOldItemDefinitionIndex = iStaticItemDefinitionIndex;
			int iNewItemDefinitionIndex = iStaticItemDefinitionIndex = pWeapon->m_iItemDefinitionIndex();

			if (iNewItemDefinitionIndex != iOldItemDefinitionIndex || !pWeapon->m_iClip1() || !pLocal->IsAlive() || pLocal->IsTaunting() || pLocal->IsBonked() || pLocal->IsAGhost() || pLocal->IsInBumperKart())
				G::WaitForShift = 1;
		}

		G::CanPrimaryAttack = G::CanSecondaryAttack = false;
		bool bCanAttack = pLocal->CanAttack();
		switch (SDK::GetRoundState())
		{
		case GR_STATE_BETWEEN_RNDS:
		case GR_STATE_GAME_OVER:
			bCanAttack = bCanAttack && !(pLocal->m_fFlags() & FL_FROZEN);
		}
		if (bCanAttack)
		{
			switch (pWeapon->GetWeaponID())
			{
			case TF_WEAPON_FLAME_BALL:
				G::CanPrimaryAttack = G::CanSecondaryAttack = pLocal->m_flTankPressure() >= 100.f; break;
			case TF_WEAPON_BUILDER:
				G::CanPrimaryAttack = true; break;
			default:
				G::CanPrimaryAttack = pWeapon->CanPrimaryAttack();
				G::CanSecondaryAttack = pWeapon->CanSecondaryAttack();

				switch (pWeapon->GetWeaponID())
				{
				case TF_WEAPON_MINIGUN:
				{
					int iState = pWeapon->As<CTFMinigun>()->m_iWeaponState();
					if (iState != AC_STATE_FIRING && iState != AC_STATE_SPINNING)
						G::CanPrimaryAttack = false;
					break;
				}
				case TF_WEAPON_FLAREGUN_REVENGE:
					if (pCmd->buttons & IN_ATTACK2)
						G::CanPrimaryAttack = false;
					break;
				case TF_WEAPON_BAT_WOOD:
				case TF_WEAPON_BAT_GIFTWRAP:
					if (!pWeapon->HasPrimaryAmmoForShot())
						G::CanSecondaryAttack = false;
					break;
				case TF_WEAPON_MEDIGUN: break;
				default:
					if (pWeapon->GetSlot() != SLOT_MELEE)
					{
						if (pWeapon->IsInReload())
							G::CanPrimaryAttack = pWeapon->HasPrimaryAmmoForShot();
						else if (pWeapon->m_iItemDefinitionIndex() != Soldier_m_TheBeggarsBazooka && !pWeapon->HasPrimaryAmmoForShot())
							G::CanPrimaryAttack = false;
					}
				}
			}
		}

		/*
		if (Vars::Aimbot::General::AimType.Value && Vars::Debug::Info.Value) // removeme
		{
			float flAttack1 = pWeapon->m_flNextPrimaryAttack();
			float flAttack2 = pLocal->m_flNextAttack();
			float flTime = TICKS_TO_TIME(pLocal->m_nTickBase());
			int iChoked = I::ClientState->chokedcommands;
			bool bAttack = G::LastUserCmd->buttons & IN_ATTACK;

			bool bTime = flAttack1 <= flTime && flAttack2 <= flTime;
			float flDiff1 = flTime - flAttack1;
			float flDiff2 = flTime - flAttack2;

			auto tColor = Vars::Menu::Theme::Accent.Value;
			tColor.a = bOldAttacking ? 255 : G::CanPrimaryAttack ? 127 : bTime ? 10 : 0;

			SDK::Output(
				"Can attack",
				std::format(
					"\n\t({} | {}) <= {}\n\t{} ({}, {})",
					flAttack1, flAttack2, flTime,
					bTime, flDiff1, flDiff2
				).c_str(),
				tColor
			);
			I::CVar->ConsoleColorPrintf(iChoked ? Vars::Menu::Theme::Accent.Value : Color_t(), std::format("\t{}", iChoked).c_str());
			I::CVar->ConsolePrintf(", ");
			I::CVar->ConsoleColorPrintf(bAttack ? Vars::Menu::Theme::Accent.Value : Color_t(), std::format("{}\n", bAttack).c_str());
			I::CVar->ConsoleColorPrintf(Color_t(), std::format("\t{}, {}\n", pWeapon->m_bInReload(), pWeapon->m_iReloadMode()).c_str());
		}
		*/

		G::IsAttacking = SDK::IsAttacking(pLocal, pWeapon, pCmd);
		G::PrimaryWeaponType = SDK::GetWeaponType(pWeapon, &G::SecondaryWeaponType);
		G::CanHeadshot = pWeapon->CanHeadShot();
	}

	// Run Features
	F::Misc.RunPre(pLocal, pCmd);
	F::Backtrack.Run(pCmd);

	F::EnginePrediction.Start(pLocal, pCmd);
	F::Aimbot.Run(pLocal, pWeapon, pCmd);
	F::EnginePrediction.End(pLocal, pCmd);

	F::PacketManip.Run(pLocal, pWeapon, pCmd, pSendPacket);
	F::Ticks.MovePost(pLocal, pCmd);
	F::CritHack.Run(pLocal, pWeapon, pCmd);
	F::NoSpread.Run(pLocal, pWeapon, pCmd);
	F::Misc.RunPost(pLocal, pCmd, *pSendPacket);
	F::Resolver.CreateMove();
	F::Visuals.CreateMove(pLocal, pWeapon);

	{
		static bool bWasSet = false;
		const bool bOverchoking = I::ClientState->chokedcommands >= 21 || G::ShiftedTicks + I::ClientState->chokedcommands == G::MaxShift + (F::AntiAim.YawOn() ? 3 : 0); // failsafe
		if (G::PSilentAngles && !bOverchoking)
			*pSendPacket = false, bWasSet = true;
		else if (bWasSet || bOverchoking)
			*pSendPacket = true, bWasSet = false;
	}
	F::Misc.DoubletapPacket(pCmd, pSendPacket);
	F::AntiAim.Run(pLocal, pWeapon, pCmd, *pSendPacket);
	G::Choking = !*pSendPacket;
	if (*pSendPacket)
		F::FakeAngle.Run(pLocal);

	G::ViewAngles = pCmd->viewangles;
	G::LastUserCmd = pCmd;

	//const bool bShouldSkip = G::PSilentAngles || G::SilentAngles || G::AntiAim || G::AvoidingBackstab;
	//return bShouldSkip ? false : CALL_ORIGINAL(rcx, edx, input_sample_frametime, pCmd);
	return false;
}