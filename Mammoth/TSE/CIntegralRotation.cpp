//	CIntegralRotation.cpp
//
//	CIntegralRotation class
//	Copyright (c) 2014 by George Moromisato. All Rights Reserved.

#include "PreComp.h"

#define FIELD_THRUSTER_POWER					CONSTLIT("thrusterPower")

const Metric MANEUVER_MASS_FACTOR =				1.0;
const Metric MAX_INERTIA_RATIO =				9.0;

ICCItemPtr CIntegralRotation::Diagnostics (int iFrameCount, Metric rMaxRotationSpeed, Metric rAccel, Metric rAccelStop)

//	Diagnostics
//
//	Returns diagnostics on implementation.

	{
	try
		{
		static constexpr int TURN_COUNT = 10;

		ICCItemPtr pResult(ICCItem::List);

		CIntegralRotationDesc Desc;
		Desc.Init(iFrameCount, rMaxRotationSpeed, rAccel, rAccelStop);

		CIntegralRotation Rotation;
		Rotation.Init(Desc, 0);

		pResult->Append(Rotation.GetStatus(Desc));

		int iStartFrame = Rotation.m_iRotationFrame;
		int iStartSpeed = Rotation.m_iRotationSpeed;

		//	Turn 10 ticks clockwise

		for (int i = 0; i < TURN_COUNT; i++)
			{
			Rotation.Update(Desc, EManeuver::RotateRight);
			pResult->Append(Rotation.GetStatus(Desc));
			}

		Rotation.SetRotationSpeedDegrees(Desc, 0.0);
		pResult->Append(Rotation.GetStatus(Desc));

		//	Turn 10 ticks counter-clockwise

		for (int i = 0; i < TURN_COUNT; i++)
			{
			Rotation.Update(Desc, EManeuver::RotateLeft);
			pResult->Append(Rotation.GetStatus(Desc));
			}

		if (Rotation.m_iRotationFrame != iStartFrame)
			pResult->Append(ICCItemPtr(CONSTLIT("Asymmetric rotation detected.")));

		Rotation.Init(Desc, 0);
		pResult->Append(Rotation.GetStatus(Desc));

		//	Turn 10 ticks counter-clockwise.

		for (int i = 0; i < TURN_COUNT; i++)
			{
			Rotation.Update(Desc, EManeuver::RotateLeft);
			pResult->Append(Rotation.GetStatus(Desc));
			}

		Rotation.SetRotationSpeedDegrees(Desc, 0.0);
		pResult->Append(Rotation.GetStatus(Desc));

		//	Turn 10 ticks clockwise

		for (int i = 0; i < TURN_COUNT; i++)
			{
			Rotation.Update(Desc, EManeuver::RotateRight);
			pResult->Append(Rotation.GetStatus(Desc));
			}

		if (Rotation.m_iRotationFrame != iStartFrame)
			pResult->Append(ICCItemPtr(CONSTLIT("Asymmetric rotation detected.")));

		//	Done

		return pResult;
		}
	catch (...)
		{
		return ICCItemPtr::Error(CONSTLIT("Crash in CIntegralRotation::Diagnostics."));
		}
	}

EManeuver CIntegralRotation::GetManeuverToFace (const CIntegralRotationDesc &Desc, int iAngle) const

//	GetManeuverToFace
//
//	Returns the maneuver required to face the given angle (or NoRotation if we're
//	already facing in that (rough) direction).

	{
	//	Convert to a frame index. NOTE: We figure out what our rotation would be
	//	if we stopped thrusting right now.

	int iCurrentFrameIndex = GetFrameIndex(CalcFinalRotationFrame(Desc));
	int iDesiredFrameIndex = Desc.GetFrameIndex(iAngle);

	//	If we're going to be in the right spot by doing nothing, then just stop
	//	rotating.

	if (iCurrentFrameIndex == iDesiredFrameIndex)
		return EManeuver::None;

	//	Otherwise, figure out how many frames we need to turn (and the 
	//	direction).

	int iFrameDiff = ClockDiff(iDesiredFrameIndex, iCurrentFrameIndex, Desc.GetFrameCount());
	int iDegreeDiff = ClockDiff(iAngle, Desc.GetRotationAngle(iCurrentFrameIndex), 360);

	//	Are we turning right?

	int iNewRotationSpeed;
	int iMaxFrameRot = (Desc.GetMaxRotationSpeed() / CIntegralRotationDesc::ROTATION_FRACTION);
	if (iFrameDiff > 0)
		{
		//	If we have a ways to go, then just turn

		if (iFrameDiff > iMaxFrameRot)
			return EManeuver::RotateRight;

		//	Otherwise we need to calculate better. Figure out what our new
		//	rotation speed will be if we turn right.

		iNewRotationSpeed = m_iRotationSpeed;
		if (iNewRotationSpeed < Desc.GetMaxRotationSpeed())
			{
			if (iNewRotationSpeed < 0)
				iNewRotationSpeed = Min(Desc.GetMaxRotationSpeed(), iNewRotationSpeed + Desc.GetRotationAccelStop());
			else
				iNewRotationSpeed = Min(Desc.GetMaxRotationSpeed(), iNewRotationSpeed + Desc.GetRotationAccel());
			}
		}

	//	Or left

	else
		{
		//	If we have a ways to go, then just turn

		if (-iFrameDiff > iMaxFrameRot)
			return EManeuver::RotateLeft;

		//	Otherwise we need a better calculation. Figure out what our new
		//	rotation speed will be if we turn left.

		iNewRotationSpeed = m_iRotationSpeed;
		if (iNewRotationSpeed > -Desc.GetMaxRotationSpeed())
			{
			if (iNewRotationSpeed > 0)
				iNewRotationSpeed = Max(-Desc.GetMaxRotationSpeed(), iNewRotationSpeed - Desc.GetRotationAccelStop());
			else
				iNewRotationSpeed = Max(-Desc.GetMaxRotationSpeed(), iNewRotationSpeed - Desc.GetRotationAccel());
			}
		}

	//	Figure out where we will end up next tick given our new rotation speed.

	int iNewRotationFrame = m_iRotationFrame;
	int iFrameMax = Desc.GetFrameCount() * CIntegralRotationDesc::ROTATION_FRACTION;
	iNewRotationFrame = (iNewRotationFrame + iNewRotationSpeed) % iFrameMax;
	if (iNewRotationFrame < 0)
		iNewRotationFrame += iFrameMax;

	int iNewFrameIndex = GetFrameIndex(Desc.CalcFinalRotationFrame(iNewRotationFrame, iNewRotationSpeed));
	int iNewDegreeDiff = ClockDiff(iAngle, Desc.GetRotationAngle(iNewFrameIndex), 360);

	//	If we're closer to the target, then do it.

	if (iNewFrameIndex == iCurrentFrameIndex || Absolute(iNewDegreeDiff) < Absolute(iDegreeDiff))
		return (iFrameDiff < 0 ? EManeuver::RotateLeft : EManeuver::RotateRight);
	else
		return EManeuver::None;
	}

int CIntegralRotation::GetRotationAngle (const CIntegralRotationDesc &Desc) const

//	GetRotationAngle
//
//	Converts from our rotation frame to an angle

	{
	return Desc.GetRotationAngle(GetFrameIndex(m_iRotationFrame));
	}

Metric CIntegralRotation::GetRotationSpeedDegrees (const CIntegralRotationDesc &Desc) const

//	GetRotationSpeed
//
//	Returns the current rotation speed in degrees per tick.
//	Negative = counterclockwise.

	{
	return (360.0 * m_iRotationSpeed) / (Desc.GetFrameCount() * CIntegralRotationDesc::ROTATION_FRACTION);
	}

ICCItemPtr CIntegralRotation::GetStatus (const CIntegralRotationDesc &Desc) const

//	GetStatus
//
//	Returns the current rotation state.

	{
	ICCItemPtr pResult(ICCItem::SymbolTable);

	pResult->SetIntegerAt(CONSTLIT("frame"), GetFrameIndex());
	pResult->SetDoubleAt(CONSTLIT("speed"), GetRotationSpeedDegrees(Desc));

	pResult->SetIntegerAt(CONSTLIT("currentFrameVar"), m_iRotationFrame);
	pResult->SetIntegerAt(CONSTLIT("currentSpeedVar"), m_iRotationSpeed);
	
	if (m_iLastManeuver == EManeuver::None)
		pResult->SetStringAt(CONSTLIT("lastManeuver"), CONSTLIT("none"));
	else if (m_iLastManeuver == EManeuver::RotateLeft)
		pResult->SetStringAt(CONSTLIT("lastManeuver"), CONSTLIT("left"));
	else if (m_iLastManeuver == EManeuver::RotateRight)
		pResult->SetStringAt(CONSTLIT("lastManeuver"), CONSTLIT("right"));
	else
		pResult->SetStringAt(CONSTLIT("lastManeuver"), CONSTLIT("unknown"));

	return pResult;
	}

void CIntegralRotation::Init (const CIntegralRotationDesc &Desc, int iRotationAngle)

//	Init
//
//	Initialize

	{
	//	Defaults

	if (iRotationAngle != -1)
		SetRotationAngle(Desc, iRotationAngle);
	else
		SetRotationAngle(Desc, 0);

	m_iRotationSpeed = 0;
	m_iLastManeuver = EManeuver::None;
	}

void CIntegralRotation::ReadFromStream (SLoadCtx &Ctx, const CIntegralRotationDesc &Desc)

//	ReadFromStream
//
//	Reads data

	{
	DWORD dwLoad;

	Init(Desc);

	Ctx.pStream->Read((char *)&dwLoad, sizeof(DWORD));
	m_iRotationFrame = (int)dwLoad;

	Ctx.pStream->Read((char *)&dwLoad, sizeof(DWORD));
	m_iRotationSpeed = (int)dwLoad;

	//	Make sure our frame is within bounds; this can change if the ship's
	//	rotation count is altered in the XML.

	if (GetFrameIndex() >= Desc.GetFrameCount())
		SetRotationAngle(Desc, 0);
	}

void CIntegralRotation::SetRotationAngle (const CIntegralRotationDesc &Desc, int iAngle)

//	SetRotationAngle
//
//	Sets the rotation angle

	{
	m_iRotationFrame = CIntegralRotationDesc::ROTATION_FRACTION * Desc.GetFrameIndex(iAngle) + (CIntegralRotationDesc::ROTATION_FRACTION / 2);
	}

void CIntegralRotation::SetRotationSpeedDegrees (const CIntegralRotationDesc &Desc, Metric rDegreesPerTick)

//	SetRotationSpeedDegree
//
//	Sets the current rotation speed.

	{
	m_iRotationSpeed = mathRound((rDegreesPerTick * Desc.GetFrameCount() * CIntegralRotationDesc::ROTATION_FRACTION) / 360.0);
	}

void CIntegralRotation::Update (const CIntegralRotationDesc &Desc, EManeuver iManeuver)

//	Update
//
//	Updates once per tick

	{
	DEBUG_TRY

	//	Change the rotation velocity

	switch (iManeuver)
		{
		case EManeuver::None:
			if (m_iRotationSpeed != 0)
				{
				//	Slow down rotation

				if (m_iRotationSpeed > 0)
					{
					m_iRotationSpeed = Max(0, m_iRotationSpeed - Desc.GetRotationAccelStop());
					m_iLastManeuver = EManeuver::RotateLeft;
					}
				else
					{
					m_iRotationSpeed = Min(0, m_iRotationSpeed + Desc.GetRotationAccelStop());
					m_iLastManeuver = EManeuver::RotateRight;
					}

				//	If we've stopped rotating, align to center of frame

				if (m_iRotationSpeed == 0)
					m_iRotationFrame = ((m_iRotationFrame / CIntegralRotationDesc::ROTATION_FRACTION) * CIntegralRotationDesc::ROTATION_FRACTION) + (CIntegralRotationDesc::ROTATION_FRACTION / 2);
				}
			else
				m_iLastManeuver = EManeuver::None;
			break;

		case EManeuver::RotateRight:
			if (m_iRotationSpeed < Desc.GetMaxRotationSpeed())
				{
				if (m_iRotationSpeed < 0)
					m_iRotationSpeed = Min(Desc.GetMaxRotationSpeed(), m_iRotationSpeed + Desc.GetRotationAccelStop());
				else
					m_iRotationSpeed = Min(Desc.GetMaxRotationSpeed(), m_iRotationSpeed + Desc.GetRotationAccel());
				m_iLastManeuver = EManeuver::RotateRight;
				}
			else
				m_iLastManeuver = EManeuver::None;
			break;

		case EManeuver::RotateLeft:
			if (m_iRotationSpeed > -Desc.GetMaxRotationSpeed())
				{
				if (m_iRotationSpeed > 0)
					m_iRotationSpeed = Max(-Desc.GetMaxRotationSpeed(), m_iRotationSpeed - Desc.GetRotationAccelStop());
				else
					m_iRotationSpeed = Max(-Desc.GetMaxRotationSpeed(), m_iRotationSpeed - Desc.GetRotationAccel());
				m_iLastManeuver = EManeuver::RotateLeft;
				}
			else
				m_iLastManeuver = EManeuver::None;
			break;
		}

	//	Now rotate

	if (m_iRotationSpeed != 0)
		{
		int iFrameMax = Desc.GetFrameCount() * CIntegralRotationDesc::ROTATION_FRACTION;

		m_iRotationFrame = (m_iRotationFrame + m_iRotationSpeed) % iFrameMax;
		if (m_iRotationFrame < 0)
			m_iRotationFrame += iFrameMax;
		}

	DEBUG_CATCH
	}

void CIntegralRotation::WriteToStream (IWriteStream *pStream) const

//	WriteToStream
//
//	Writes data

	{
	DWORD dwSave = m_iRotationFrame;
	pStream->Write((char *)&dwSave, sizeof(DWORD));

	dwSave = m_iRotationSpeed;
	pStream->Write((char *)&dwSave, sizeof(DWORD));
	}
