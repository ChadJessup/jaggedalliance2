#ifdef PRECOMPILEDHEADERS
	#include "Strategic All.h"
#else
	#include "Types.h"
	#include "Merc Contract.h"
	#include "Soldier Profile.h"
	#include "History.h"
	#include "finances.h"
	#include "Game Clock.h"
	#include "Soldier Add.h"
	#include "Dialogue Control.h"
	#include "Soldier Create.h"
	#include "Message.h"
	#include "Font Control.h"
	#include "personnel.h"
	#include "LaptopSave.h"
	#include "Map Screen Interface.h"
	#include "strategic.h"
	#include "strategicmap.h"
	#include "Quests.h"
	#include "worlddef.h"	
	#include "rotting corpses.h"
	#include "Animation Control.h"
	#include "Tactical Save.h"
	#include "Strategic Merc Handler.h"
	#include "Interface Control.h"
	#include "gamescreen.h"
	#include "jascreens.h"
	#include "Random.h"
	#include "Assignments.h"
	#include "Strategic Movement.h"
	#include "Squads.h"
	#include "gameloop.h"
	#include "Text.h"
	#include "Strategic Status.h"
	#include "mercs.h"
	#include "insurance contract.h"
#endif


void CalculateMedicalDepositRefund( SOLDIERTYPE *pSoldier );
void NotifyPlayerOfMercDepartureAndPromptEquipmentPlacement( SOLDIERTYPE *pSoldier, BOOLEAN fAddRehireButton );
void MercDepartEquipmentBoxCallBack( UINT8 bExitValue );
BOOLEAN HandleFiredDeadMerc( SOLDIERTYPE *pSoldier );
void HandleExtendMercsContract( SOLDIERTYPE *pSoldier );
void HandleSoldierLeavingWithLowMorale( SOLDIERTYPE *pSoldier );
void HandleSoldierLeavingForAnotherContract( SOLDIERTYPE *pSoldier );
//BOOLEAN SoldierWantsToDelayRenewalOfContract( SOLDIERTYPE *pSoldier );
void HandleNotifyPlayerCantAffordInsurance( void );
void HandleNotifyPlayerCanAffordInsurance( SOLDIERTYPE *pSoldier, UINT8 ubLength, INT32 iCost );
void ExtendMercInsuranceContractCallBack( UINT8 bExitValue );
void HandleUniqueEventWhenPlayerLeavesTeam( SOLDIERTYPE *pSoldier );


UINT32 uiContractTimeMode = 0;

SOLDIERTYPE *pLeaveSoldier = NULL;

BOOLEAN	fEnterMapDueToContract = FALSE;
extern BOOLEAN fOneFrame;
extern BOOLEAN fPausedTimeDuringQuote;
UINT8 ubQuitType = 0;
BOOLEAN gfFirstMercSayQuote  = FALSE;
extern BOOLEAN gfFirstMercSayingQuoteWillLeaveNoMatterWhat;

extern CHAR16		gzUserDefinedButton1[ 128 ];
extern CHAR16		gzUserDefinedButton2[ 128 ];


SOLDIERTYPE *pContractReHireSoldier = NULL;

UINT8		gubContractLength = 0;		//used when extending a mercs insurance contract
SOLDIERTYPE *gpInsuranceSoldier=NULL;

// The values need to be saved!
CONTRACT_NEWAL_LIST_NODE	ContractRenewalList[ 20 ];
UINT8											ubNumContractRenewals = 0;
// end
UINT8											ubCurrentContractRenewal = 0;
UINT8											ubCurrentContractRenewalInProgress = FALSE;
BOOLEAN										gfContractRenewalSquenceOn = FALSE;		
BOOLEAN										gfInContractMenuFromRenewSequence = FALSE;
 

// the airport sector
#define AIRPORT_X 13 
#define AIRPORT_Y 2


BOOLEAN SaveContractRenewalDataToSaveGameFile( HWFILE hFile )
{
	UINT32	uiNumBytesWritten;

	FileWrite( hFile, ContractRenewalList, sizeof( ContractRenewalList ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( ContractRenewalList ) )
	{
		return( FALSE );
	}

	FileWrite( hFile, &ubNumContractRenewals, sizeof( ubNumContractRenewals ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( ubNumContractRenewals ) )
	{
		return( FALSE );
	}


  return( TRUE );
}


BOOLEAN LoadContractRenewalDataFromSaveGameFile( HWFILE hFile )
{
	UINT32	uiNumBytesRead;

	FileRead( hFile, ContractRenewalList, sizeof( ContractRenewalList ), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( ContractRenewalList ) )
	{
		return( FALSE );
	}

	FileRead( hFile, &ubNumContractRenewals, sizeof( ubNumContractRenewals ), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( ubNumContractRenewals ) )
	{
		return( FALSE );
	}

  return( TRUE );
}

void BeginContractRenewalSequence( )
{
	INT32 cnt;
	SOLDIERTYPE *pSoldier;
	BOOLEAN			fFoundAtLeastOne = FALSE;

	if ( ubNumContractRenewals > 0)
	{
		for ( cnt = 0; cnt < ubNumContractRenewals; cnt++ )
		{
			// Get soldier - if there is none, adavance to next
			pSoldier = FindSoldierByProfileID( ContractRenewalList[ cnt ].ubProfileID, FALSE ); // Steve Willis, 80

			if ( pSoldier )
			{
				if( ( pSoldier->bActive == FALSE ) || ( pSoldier->bLife == 0 ) || ( pSoldier->bAssignment == IN_TRANSIT ) ||( pSoldier->bAssignment == ASSIGNMENT_POW ) )
				{
					// no
					continue;
				}

				// Double check there are valid people here that still want to renew...
				//if the user hasnt renewed yet, and is still leaving today
				if ( ContractIsExpiring( pSoldier ) )
				{
					fFoundAtLeastOne = TRUE;
				}
			}
		}

		if ( fFoundAtLeastOne )
		{
			// Set sequence on...
			gfContractRenewalSquenceOn = TRUE;

			// Start at first one....
			ubCurrentContractRenewal = 0;
			ubCurrentContractRenewalInProgress = 0;

			PauseGame( );
			LockPauseState( 7 );
			InterruptTime( );

			// Go into mapscreen if not already...
			SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_ENTER_MAPSCREEN,0,0,0,0,0 );
		}
	}
}


void HandleContractRenewalSequence( )
{
	SOLDIERTYPE *pSoldier;

	if ( gfContractRenewalSquenceOn )
	{
		// Should we stop now?
		if ( ubCurrentContractRenewal == ubNumContractRenewals )
		{
			// Stop and clear any on list...
			ubNumContractRenewals = 0;
			gfContractRenewalSquenceOn = FALSE;
		}

		// Get soldier - if there is none, adavance to next
		pSoldier = FindSoldierByProfileID( ContractRenewalList[ ubCurrentContractRenewal ].ubProfileID, FALSE ); // Steve Willis, 80

		if ( pSoldier == NULL )
		{
			// Advance to next guy!
			EndCurrentContractRenewal( );
			return;
		}

		// OK, check if it's in progress...
		if ( !ubCurrentContractRenewalInProgress )
		{
			// Double check contract situation....
			if ( ContractIsExpiring( pSoldier ) )
			{
				// Set this one in motion!
				ubCurrentContractRenewalInProgress = 1;

				// Handle start here...

				// Determine what quote to use....
				if( !WillMercRenew( pSoldier, FALSE ) )
				{
					// OK, he does not want to renew.......
					HandleImportantMercQuote( pSoldier, QUOTE_MERC_LEAVING_ALSUCO_SOON );

					// Do special dialogue event...
					SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_CONTRACT_NOGO_TO_RENEW , pSoldier->ubID,0 ,0 ,0 ,0 );
				}
				else
				{		
					// OK check what dialogue to play
					// If we have not used this one before....
					if ( pSoldier->ubContractRenewalQuoteCode == SOLDIER_CONTRACT_RENEW_QUOTE_NOT_USED )
					{
						SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,1 ,MAP_SCREEN ,0 ,0 ,0 );
						HandleImportantMercQuote( pSoldier, QUOTE_CONTRACTS_OVER );
						SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,0 ,MAP_SCREEN ,0 ,0 ,0 );
					}
					// Else if we have said 89 already......
					else if ( pSoldier->ubContractRenewalQuoteCode == SOLDIER_CONTRACT_RENEW_QUOTE_89_USED )
					{
						SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,1 ,MAP_SCREEN ,0 ,0 ,0 );
						HandleImportantMercQuote( pSoldier, QUOTE_MERC_LEAVING_ALSUCO_SOON );
						SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,0 ,MAP_SCREEN ,0 ,0 ,0 );
					}
					
					// Do special dialogue event...
					SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_CONTRACT_WANTS_TO_RENEW , pSoldier->ubID,0 ,0 ,0 ,0 );
				}
			}
			else
			{
				// Skip to next guy!
				EndCurrentContractRenewal( );
			}
		}
	}
}

void EndCurrentContractRenewal( )
{
	// Are we in the requence?
	if ( gfContractRenewalSquenceOn )
	{
		// OK stop this one and increment current one
		ubCurrentContractRenewalInProgress = FALSE;
		gfInContractMenuFromRenewSequence	 = FALSE;

		ubCurrentContractRenewal++;

	}
}

void HandleMercIsWillingToRenew( UINT8 ubID )
{
	SOLDIERTYPE *pSoldier = MercPtrs[ ubID ];

	// We wish to lock interface
	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,1,MAP_SCREEN,0,0,0 );

	CheckIfSalaryIncreasedAndSayQuote( pSoldier, FALSE );

	// Setup variable for this....
	gfInContractMenuFromRenewSequence = TRUE;

	// Show contract menu
	TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_SHOW_CONTRACT_MENU, 0,0 );

	// Unlock now
	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,0 ,MAP_SCREEN ,0 ,0 ,0 );
}


void HandleMercIsNotWillingToRenew( UINT8 ubID )
{
	SOLDIERTYPE *pSoldier = MercPtrs[ ubID ];

	// We wish to lock interface
	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,1,MAP_SCREEN,0,0,0 );
	
	// Setup variable for this....
	gfInContractMenuFromRenewSequence = TRUE;

	// Show contract menu
	TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_SHOW_CONTRACT_MENU, 0,0 );

	// Unlock now
	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,0 ,MAP_SCREEN ,0 ,0 ,0 );

}


// This is used only to EXTEND the contract of an AIM merc already on the team
BOOLEAN	MercContractHandling( SOLDIERTYPE	*pSoldier, UINT8 ubDesiredAction )
{
	INT32	iContractCharge=0;
	INT32	iContractLength=0;
	UINT8	ubHistoryContractType=0;
	UINT8	ubFinancesContractType=0;
	INT32 iCostOfInsurance = 0;


	//determins what kind of merc the contract is being extended for (only aim mercs can extend contract)
	if( pSoldier->ubWhatKindOfMercAmI != MERC_TYPE__AIM_MERC )
		return(FALSE);

	switch( ubDesiredAction )
	{
		case CONTRACT_EXTEND_1_DAY:
			//check to see if the merc has enough money
			iContractCharge = gMercProfiles[ pSoldier->ubProfile ].sSalary;

			//set the contract length and the charge
			iContractLength = 1;

			ubHistoryContractType = HISTORY_EXTENDED_CONTRACT_1_DAY;
			ubFinancesContractType = EXTENDED_CONTRACT_BY_1_DAY;
			break;


		case CONTRACT_EXTEND_1_WEEK:
			iContractCharge = gMercProfiles[ pSoldier->ubProfile ].uiWeeklySalary;

			//set the contract length and the charge
			iContractLength = 7;

			ubHistoryContractType = HISTORY_EXTENDED_CONTRACT_1_WEEK;
			ubFinancesContractType = EXTENDED_CONTRACT_BY_1_WEEK;
			break;


		case CONTRACT_EXTEND_2_WEEK:
			iContractCharge = gMercProfiles[ pSoldier->ubProfile ].uiBiWeeklySalary;

			//set the contract length and the charge
			iContractLength = 14;

			ubHistoryContractType = HISTORY_EXTENDED_CONTRACT_2_WEEK;
			ubFinancesContractType = EXTENDED_CONTRACT_BY_2_WEEKS;
			break;

		default:
			return(FALSE);
			break;
	}

	//check to see if the merc has enough money
	if( LaptopSaveInfo.iCurrentBalance < iContractCharge )
		return(FALSE);

	//Check to see if merc will renew
	if( !WillMercRenew( pSoldier, TRUE ) )
	{
		// Remove soldier.... ( if this is setup because normal contract ending dequence... )
		if ( ContractIsExpiring( pSoldier ) )
		{
			TacticalCharacterDialogueWithSpecialEvent( pSoldier, 0, DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING, 1,0 );
		}
		return(FALSE);
	}

	
	fPausedTimeDuringQuote = TRUE;

	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,1 ,MAP_SCREEN ,0 ,0 ,0 );

	//
	// These calcs need to be done before Getting/Calculating the insurance costs
	//

	//set the contract length and the charge
	pSoldier->iTotalContractLength += iContractLength;
//	pSoldier->iTotalContractCharge = iContractCharge;
	pSoldier->bTypeOfLastContract = ubDesiredAction;

	//determine the end of the contract
	pSoldier->iEndofContractTime += ( iContractLength * 1440 );

	if( ( pSoldier->usLifeInsurance ) && ( pSoldier->bAssignment != ASSIGNMENT_POW ) )	//  DEF:  Removed cause they can extend a 1 day contract && ( iContractLength > 1 )
	{
		// check if player can afford insurance, if not, tell them
		iCostOfInsurance = CalculateInsuranceContractCost( iContractLength, pSoldier->ubProfile );

		HandleImportantMercQuote( pSoldier, QUOTE_ACCEPT_CONTRACT_RENEWAL );

		if( iCostOfInsurance > LaptopSaveInfo.iCurrentBalance )
		{
			// no can afford
			HandleNotifyPlayerCantAffordInsurance( );

			// OK, handle ending of renew session
			if ( gfInContractMenuFromRenewSequence )
			{
				EndCurrentContractRenewal( );
			}

		}
		else
		{
			// can afford ask if they want it
			HandleNotifyPlayerCanAffordInsurance( pSoldier, ( UINT8 )( iContractLength ), iCostOfInsurance );
		}
	}
	else
	{
		// no need to query for life insurance
		HandleImportantMercQuote( pSoldier, QUOTE_ACCEPT_CONTRACT_RENEWAL );

		// OK, handle ending of renew session
		if ( gfInContractMenuFromRenewSequence )
		{
			EndCurrentContractRenewal( );
		}

	}

	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,0 ,MAP_SCREEN ,0 ,0 ,0 );


  // ATE: Setup when they can be signed again!
  // If they are 2-weeks this can be extended
  // otherwise don't change from current
  if ( pSoldier->bTypeOfLastContract == CONTRACT_EXTEND_2_WEEK )
  {
    pSoldier->iTimeCanSignElsewhere = pSoldier->iEndofContractTime;
  }

// ARM: Do not reset because of renewal!  The quote is for early dismissal from *initial* time of hiring
//	pSoldier->uiTimeOfLastContractUpdate = GetWorldTotalMin();

// ARM: Do not reset because of renewal!  The deposit in the profile goes up when merc levels, but the one in the soldier
// structure must always reflect the deposit actually paid (which does NOT change when a merc levels).
//	pSoldier->usMedicalDeposit = gMercProfiles[ pSoldier->ubProfile ].sMedicalDepositAmount;

	//add an entry in the finacial page for the extending  of the mercs contract
	AddTransactionToPlayersBook( ubFinancesContractType, pSoldier->ubProfile, GetWorldTotalMin(), -iContractCharge );

	//add an entry in the history page for the extending of the merc contract
	AddHistoryToPlayersLog( ubHistoryContractType, pSoldier->ubProfile, GetWorldTotalMin(), pSoldier->sSectorX, pSoldier->sSectorY );

	return( TRUE );
}


BOOLEAN WillMercRenew( SOLDIERTYPE	*pSoldier, BOOLEAN fSayQuote )
{
	UINT8	i;
	INT8	bMercID;
	BOOLEAN fBuddyAround = FALSE;
	BOOLEAN fUnhappy = FALSE;
	UINT16 usBuddyQuote=0;
	UINT16 usReasonQuote=0;
	BOOLEAN fSayPrecedent = FALSE;
	SOLDIERTYPE * pHated;

	if( pSoldier->ubWhatKindOfMercAmI != MERC_TYPE__AIM_MERC )
		return( FALSE );

	// does the merc have another contract already lined up?
	if( pSoldier->fSignedAnotherContract )
	{
		// NOTE: Having a buddy around will NOT stop a merc from leaving on another contract (IC's call)

		if( fSayQuote == TRUE )
		{
			SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,1 ,MAP_SCREEN ,0 ,0 ,0 );
			HandleImportantMercQuote( pSoldier, QUOTE_WONT_RENEW_CONTRACT_LAME_REFUSAL );
			SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,0 ,MAP_SCREEN ,0 ,0 ,0 );
		}
		return( FALSE );
	}

	// find out if the merc has a buddy working for the player
	// loop through the list of people the merc considers buddies
	for(i=0; i<5; i++)
	{
		bMercID = gMercProfiles[ pSoldier->ubProfile ].bBuddy[i];

		if( bMercID < 0 )
			continue;

		// is this buddy on the team?
		if( IsMercOnTeamAndAlive( (UINT8) bMercID) )
		{
			fBuddyAround = TRUE;

			if( i == 0 )
				usBuddyQuote = QUOTE_RENEWING_CAUSE_BUDDY_1_ON_TEAM;
			else if( i == 1 )
				usBuddyQuote = QUOTE_RENEWING_CAUSE_BUDDY_2_ON_TEAM;
			else
				usBuddyQuote = QUOTE_RENEWING_CAUSE_LEARNED_TO_LIKE_BUDDY_ON_TEAM;

			// use first buddy in case there are multiple
			break;
		}
	}


	// WE CHECK FOR SOURCES OF UNHAPPINESS IN ORDER OF IMPORTANCE, which is:
	// 1) Hated Mercs (Highest), 2) Death Rate, 3) Morale (lowest)


	// see if someone the merc hates is on the team
	// loop through the list of people the merc hates
	for(i=0; i<2; i++)
	{
		bMercID = gMercProfiles[ pSoldier->ubProfile ].bHated[ i ];

		if( bMercID < 0 )
			continue;

		if( IsMercOnTeamAndInOmertaAlreadyAndAlive( (UINT8) bMercID ) )
		{
			if ( gMercProfiles[ pSoldier->ubProfile ].bHatedCount[ i ] == 0 )
			{
				// our tolerance has run out!
				fUnhappy = TRUE;
			}
			else 			// else tolerance is > 0, only gripe if in same sector
			{
				pHated = FindSoldierByProfileID( bMercID, TRUE );
				if ( pHated && pHated->sSectorX == pSoldier->sSectorX &&
									pHated->sSectorY == pSoldier->sSectorY &&
									pHated->bSectorZ == pSoldier->bSectorZ )
				{
					fUnhappy = TRUE;
				}
			}

			if ( fUnhappy )
			{
				if( i == 0 )
					usReasonQuote = QUOTE_HATE_MERC_1_ON_TEAM_WONT_RENEW;
				else
					usReasonQuote = QUOTE_HATE_MERC_2_ON_TEAM_WONT_RENEW;

				// use first hated in case there are multiple
				break;
			}
		}
	}


	if ( !fUnhappy )
	{
		// now check for learn to hate
		bMercID = gMercProfiles[ pSoldier->ubProfile ].bLearnToHate;

		if ( bMercID >= 0 )
		{
			if ( IsMercOnTeamAndInOmertaAlreadyAndAlive( (UINT8) bMercID ) )
			{				
				if ( gMercProfiles[ pSoldier->ubProfile ].bLearnToHateCount == 0 )
				{
					// our tolerance has run out!
					fUnhappy = TRUE;
					usReasonQuote = QUOTE_LEARNED_TO_HATE_MERC_1_ON_TEAM_WONT_RENEW;

        }
				else if ( gMercProfiles[ pSoldier->ubProfile ].bLearnToHateCount <= gMercProfiles[ pSoldier->ubProfile ].bLearnToHateTime / 2 )
				{
					pHated = FindSoldierByProfileID( bMercID, TRUE );
					if ( pHated && pHated->sSectorX == pSoldier->sSectorX &&
										pHated->sSectorY == pSoldier->sSectorY &&
										pHated->bSectorZ == pSoldier->bSectorZ )
					{
						fUnhappy = TRUE;
						usReasonQuote = QUOTE_LEARNED_TO_HATE_MERC_1_ON_TEAM_WONT_RENEW;
					}
				}
			}
		}
	}

	// happy so far?
	if (!fUnhappy)
	{
		// check if death rate is too high
		if( MercThinksDeathRateTooHigh( pSoldier-> ubProfile ) )
		{
			fUnhappy = TRUE;
			usReasonQuote = QUOTE_DEATH_RATE_RENEWAL;
		}
	}

	// happy so far?
	if (!fUnhappy)
	{
		// check if morale is too low
		if( MercThinksHisMoraleIsTooLow( pSoldier ) )
		{
			fUnhappy = TRUE;
			usReasonQuote = QUOTE_REFUSAL_RENEW_DUE_TO_MORALE;
		}
	}

	// say the precedent?
	fSayPrecedent = FALSE;

	// check if we say the precdent for merc
	if( fSayQuote )
	{
		if (fUnhappy)
		{
			if (fBuddyAround)
			{
				if( GetMercPrecedentQuoteBitStatus( pSoldier->ubProfile , GetQuoteBitNumberFromQuoteID( ( UINT32 ) ( usBuddyQuote ) ) ) == TRUE )
				{
					fSayPrecedent = TRUE;
				}
				else
				{
					SetMercPrecedentQuoteBitStatus( pSoldier->ubProfile , GetQuoteBitNumberFromQuoteID( ( UINT32 ) ( usBuddyQuote ) ) );
				}
			}
			else
			{
				if( GetMercPrecedentQuoteBitStatus( pSoldier->ubProfile , GetQuoteBitNumberFromQuoteID( ( UINT32 ) ( usReasonQuote ) ) ) == TRUE )
				{
					fSayPrecedent = TRUE;
				}
				else
				{
					SetMercPrecedentQuoteBitStatus( pSoldier->ubProfile , GetQuoteBitNumberFromQuoteID( ( UINT32 ) ( usReasonQuote ) )  );
				}
			}
		}
	}

	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,1 ,MAP_SCREEN ,0 ,0 ,0 );

	if( fSayPrecedent )
	{
		HandleImportantMercQuote( pSoldier, QUOTE_PRECEDENT_TO_REPEATING_ONESELF_RENEW );
	}

	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,0 ,MAP_SCREEN ,0 ,0 ,0 );

	// OK, we got all our info, let's make some decisions!
	if (fUnhappy)
	{
		if (fBuddyAround)
		{
			// unhappy, but buddy's around, so will agree to renew, but tell us why we're doing it
			if( fSayQuote == TRUE )
			{
				SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,1,MAP_SCREEN,0,0,0 );
				HandleImportantMercQuote( pSoldier, usBuddyQuote);
				SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,0,MAP_SCREEN,0,0,0 );

			}
			return( TRUE );
		}
		else
		{
			// unhappy, no buddies, will refuse to renew
			if( fSayQuote == TRUE )
			{
				SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,1 ,MAP_SCREEN ,0 ,0 ,0 );

/* ARM: Delay quote too vague, no longer to be used
				if( ( SoldierWantsToDelayRenewalOfContract( pSoldier ) ) )
				{
					// has a new job lined up
					HandleImportantMercQuote( pSoldier, QUOTE_DELAY_CONTRACT_RENEWAL );	
				}
				else
*/
				{
					Assert( usReasonQuote != 0 );
					HandleImportantMercQuote( pSoldier, usReasonQuote );
				}
				SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,0 ,MAP_SCREEN ,0 ,0 ,0 );

			}
			return(FALSE);
		}
	}
	else
	{
		// happy, no problem
		return( TRUE );
	}
}


void HandleSoldierLeavingWithLowMorale( SOLDIERTYPE *pSoldier )
{
	if( MercThinksHisMoraleIsTooLow( pSoldier ) )
	{
		// this will cause him give us lame excuses for a while until he gets over it
		// 3-6 days (but the first 1-2 days of that are spent "returning" home)
		gMercProfiles[ pSoldier->ubProfile ].ubDaysOfMoraleHangover = (UINT8) (3 + Random(4));
	}
}


void HandleSoldierLeavingForAnotherContract( SOLDIERTYPE *pSoldier )
{
	if (pSoldier->fSignedAnotherContract)
	{
		// merc goes to work elsewhere
		gMercProfiles[ pSoldier->ubProfile ].bMercStatus = MERC_WORKING_ELSEWHERE;
		gMercProfiles[ pSoldier->ubProfile ].uiDayBecomesAvailable += 1 + Random(6 + (pSoldier->bExpLevel / 2) );		// 1-(6 to 11) days
	}
}



/*
BOOLEAN SoldierWantsToDelayRenewalOfContract( SOLDIERTYPE *pSoldier )
{

	INT8 bTypeOfCurrentContract = 0; // what kind of contract the merc has..1 day, week or 2 week
	INT32 iLeftTimeOnContract = 0; // how much time til contract expires..in minutes
	INT32 iToleranceLevelForContract = 0; // how much time before contract ends before merc actually speaks thier mind

	// does the soldier want to delay renew of contract, possibly due to poor performance by player
	if( pSoldier->ubWhatKindOfMercAmI != MERC_TYPE__AIM_MERC )
		return( FALSE );
	
	// type of contract the merc had
	bTypeOfCurrentContract = pSoldier -> bTypeOfLastContract;
	iLeftTimeOnContract = pSoldier->iEndofContractTime - GetWorldTotalMin();
	
	// grab tolerance
	switch( bTypeOfCurrentContract )
	{
		case( CONTRACT_EXTEND_1_DAY ):
			// 20 hour tolerance on 24 hour contract
			iToleranceLevelForContract = 20 * 60;
			break;
		case( CONTRACT_EXTEND_1_WEEK ):
			// two day tolerance for 1 week
			iToleranceLevelForContract = 2 * 24 * 60;
			break;
		case( CONTRACT_EXTEND_2_WEEK ):
			// three day on 2 week contract
			iToleranceLevelForContract = 3 * 24 * 60;
			break;
	}

	if( iLeftTimeOnContract > iToleranceLevelForContract )
	{
		return( TRUE );
	}
	else
	{
		return( FALSE );
	}

}
*/


// this is called once a day (daily update) for every merc working for the player
void CheckIfMercGetsAnotherContract( SOLDIERTYPE *pSoldier )
{
	UINT32 uiFullDaysRemaining = 0;
	INT32 iChance = 0;

	// aim merc?
	if( pSoldier->ubWhatKindOfMercAmI != MERC_TYPE__AIM_MERC )
		return;

  // ATE: check time we have and see if we can accept new contracts....
  if ( GetWorldTotalMin() <= (UINT32) pSoldier->iTimeCanSignElsewhere )
  {
    return;
  }

	// if he doesn't already have another contract
	if (!pSoldier->fSignedAnotherContract)
	{
		// chance depends on how much time he has left in his contract, and his experience level (determines demand)
		uiFullDaysRemaining = (pSoldier->iEndofContractTime - GetWorldTotalMin()) / (24 * 60);

		if (uiFullDaysRemaining == 0)
		{
			// less than a full day left on contract
			// calc the chance merc will get another contract while working for ya (this is rolled once/day)
			iChance = 3;
		}
		else
		if (uiFullDaysRemaining == 1)
		{
			// < 2 days left
			iChance = 2;
		}
		else
		if (uiFullDaysRemaining == 2)
		{
			// < 3 days left
			iChance = 1;
		}
		else
		{
			// 3+ days
			iChance = 0;
		}

		// multiply by experience level
		iChance *= pSoldier->bExpLevel;

		if( (INT32) Random( 100 ) < iChance )
		{
			// B'bye!
			pSoldier->fSignedAnotherContract = TRUE;
		}
	}
}


//for ubRemoveType pass in the enum from the .h, 	( MERC_QUIT, MERC_FIRED  )
BOOLEAN BeginStrategicRemoveMerc( SOLDIERTYPE *pSoldier, BOOLEAN fAddRehireButton )
{
	InterruptTime( );
	PauseGame();
	LockPauseState( 8 );
	
	//if the soldier may have some special action when he/she leaves the party, handle it
	HandleUniqueEventWhenPlayerLeavesTeam( pSoldier );		

  // IF the soldier is an EPC, don't ask about equipment
  if ( pSoldier->ubWhatKindOfMercAmI == MERC_TYPE__EPC )
  {
    UnEscortEPC( pSoldier );
  }
  else
  {
	  NotifyPlayerOfMercDepartureAndPromptEquipmentPlacement( pSoldier, fAddRehireButton );
  }

	return( TRUE );
}

BOOLEAN StrategicRemoveMerc( SOLDIERTYPE *pSoldier )
{
	UINT8	ubHistoryCode=0;


	if ( gfInContractMenuFromRenewSequence )
	{
		EndCurrentContractRenewal( );
	}
	
	// ATE: Determine which HISTORY ENTRY to use...
	if ( pSoldier->ubLeaveHistoryCode == 0 )
	{
		// Default use contract expired reason...
		pSoldier->ubLeaveHistoryCode = HISTORY_MERC_CONTRACT_EXPIRED;
	}

	ubHistoryCode = pSoldier->ubLeaveHistoryCode;

	//if the soldier is DEAD
	if( pSoldier->bLife <= 0 )
	{
		AddCharacterToDeadList( pSoldier );
	}

	//else if the merc was fired
	else if( ubHistoryCode == HISTORY_MERC_FIRED || pSoldier->bAssignment == ASSIGNMENT_POW )
	{
		AddCharacterToFiredList( pSoldier );
	}

	//The merc is leaving for some other reason
	else
	{
		AddCharacterToOtherList( pSoldier );
	}


	if( pSoldier->ubWhatKindOfMercAmI == MERC_TYPE__NPC )
	{
		SetupProfileInsertionDataForSoldier( pSoldier ); 
	}

	//remove him from the soldier structure
	if( pSoldier->bAssignment >= ON_DUTY )
	{
		// is he/she in a mvt group, if so, remove and destroy the group
		if( pSoldier->ubGroupID )
		{
			if ( pSoldier->bAssignment != VEHICLE )
			{ //Can only remove groups if they aren't persistant (not in a squad or vehicle)
				RemoveGroup( pSoldier->ubGroupID );
			}
			else
			{
				// remove him from any existing merc slot he could be in
				RemoveMercSlot( pSoldier );
        TakeSoldierOutOfVehicle( pSoldier );
			}
		}

	}
	else
	{
		RemoveCharacterFromSquads( pSoldier );
	}

	// if the merc is not dead
	if( gMercProfiles[ pSoldier->ubProfile ].bMercStatus != MERC_IS_DEAD )
	{
		//Set the status to returning home ( delay the merc for rehire )
		gMercProfiles[ pSoldier->ubProfile ].bMercStatus = MERC_RETURNING_HOME;

		// specify how long the merc will continue to be unavailable
		gMercProfiles[ pSoldier->ubProfile ].uiDayBecomesAvailable = 1 + Random(2);		// 1-2 days

		HandleSoldierLeavingWithLowMorale( pSoldier );
		HandleSoldierLeavingForAnotherContract( pSoldier );
	}


	//add an entry in the history page for the firing/quiting of the merc
	// ATE: Don't do this if they are already dead!
	if ( !( pSoldier->uiStatusFlags & SOLDIER_DEAD ) )
	{
		AddHistoryToPlayersLog( ubHistoryCode, pSoldier->ubProfile, GetWorldTotalMin(), pSoldier->sSectorX, pSoldier->sSectorY );
	}

	//if the merc was a POW, remember it becuase the merc cant show up in AIM or MERC anymore
	if( pSoldier->bAssignment	== ASSIGNMENT_POW )
	{
		gMercProfiles[ pSoldier->ubProfile ].bMercStatus = MERC_FIRED_AS_A_POW;
	}

	//else the merc CAN get his medical deposit back
	else
	{
		//Determine how much of a Medical deposit is going to be refunded to the player
		CalculateMedicalDepositRefund( pSoldier );
	}

	//remove the merc from the tactical 
	TacticalRemoveSoldier( pSoldier->ubID );

	// Check if we should remove loaded world...
	CheckAndHandleUnloadingOfCurrentWorld();

	if ( guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN )
	{
		ReBuildCharactersList( );
	}

	fMapPanelDirty = TRUE;
	fTeamPanelDirty = TRUE;
	fCharacterInfoPanelDirty = TRUE;

	// stop time compression so player can react to the departure
	StopTimeCompression();

  // ATE: update team panels....
  UpdateTeamPanelAssignments( );

	return( TRUE );
}


void CalculateMedicalDepositRefund( SOLDIERTYPE *pSoldier )
{
	INT32		iRefundAmount=0;

	//if the merc didnt have any medical deposit, exit
	if( !gMercProfiles[ pSoldier->ubProfile ].bMedicalDeposit )
		return;

	//if the merc is at full health, refund the full medical deposit
	if( pSoldier->bLife == pSoldier->bLifeMax )
	{
		//add an entry in the finacial page for the FULL refund of the medical deposit
		// use the medical deposit in pSoldier, not in profile, which goes up with leveling
		AddTransactionToPlayersBook(FULL_MEDICAL_REFUND, pSoldier->ubProfile, GetWorldTotalMin(), pSoldier->usMedicalDeposit );

		//add an email
		AddEmailWithSpecialData( AIM_MEDICAL_DEPOSIT_REFUND, AIM_MEDICAL_DEPOSIT_REFUND_LENGTH, AIM_SITE, GetWorldTotalMin(), pSoldier->usMedicalDeposit, pSoldier->ubProfile );
	}
	//else if the merc is a dead, refund NOTHING!!
	else if( pSoldier->bLife <= 0 )
	{
		//add an entry in the finacial page for NO refund of the medical deposit
		//AddTransactionToPlayersBook( NO_MEDICAL_REFUND, pSoldier->ubProfile, GetWorldTotalMin(), 0 );

		//add an email
		AddEmailWithSpecialData( AIM_MEDICAL_DEPOSIT_NO_REFUND, AIM_MEDICAL_DEPOSIT_NO_REFUND_LENGTH, AIM_SITE, GetWorldTotalMin(), pSoldier->usMedicalDeposit, pSoldier->ubProfile );

	}
	//else the player is injured, refund a partial amount
	else
	{
		// use the medical deposit in pSoldier, not in profile, which goes up with leveling
		iRefundAmount = (INT32) ( ( pSoldier->bLife / ( FLOAT ) pSoldier->bLifeMax ) * pSoldier->usMedicalDeposit + 0.5 );

		//add an entry in the finacial page for a PARTIAL refund of the medical deposit
		AddTransactionToPlayersBook( PARTIAL_MEDICAL_REFUND, pSoldier->ubProfile, GetWorldTotalMin(), iRefundAmount );

		//add an email
		AddEmailWithSpecialData( AIM_MEDICAL_DEPOSIT_PARTIAL_REFUND, AIM_MEDICAL_DEPOSIT_PARTIAL_REFUND_LENGTH, AIM_SITE, GetWorldTotalMin(), iRefundAmount, pSoldier->ubProfile );
	}
}



void NotifyPlayerOfMercDepartureAndPromptEquipmentPlacement( SOLDIERTYPE *pSoldier, BOOLEAN fAddRehireButton )
{
	// will tell player this character is leaving and ask where they want the equipment left
	CHAR16 sString[ 1024 ];
	BOOLEAN fInSector = FALSE;
//	INT16					zTownIDString[50];
	CHAR16				zShortTownIDString[ 50 ];

	// use YES/NO Pop up box, settup for particular screen
	SGPRect pCenteringRect= {0, 0, 640, 480};

	//GetSectorIDString( pSoldier->sSectorX, pSoldier->sSectorY, pSoldier->bSectorZ, zTownIDString, TRUE );

	GetShortSectorString( pSoldier->sSectorX ,pSoldier->sSectorY, zShortTownIDString );

	// Set string for generic button
	wprintf( gzUserDefinedButton1, L"%s", zShortTownIDString );


	pLeaveSoldier = pSoldier;

	if( pSoldier->fSignedAnotherContract == TRUE )
	{
		fAddRehireButton = FALSE;
	}

	if( pSoldier->fSignedAnotherContract == TRUE )
	{
		fAddRehireButton = FALSE;
	}

	if( pSoldier->ubWhatKindOfMercAmI != MERC_TYPE__AIM_MERC )
	{
		fAddRehireButton = FALSE;
	}

	//if the character is an RPC
	if( pSoldier->ubProfile >= FIRST_RPC && pSoldier->ubProfile < FIRST_NPC )
	{
		if( gMercProfiles[ pSoldier->ubProfile ].bSex == MALE )
		{
			wprintf( sString, pMercHeLeaveString[ 4 ], pSoldier->name, zShortTownIDString );
		}
		else
		{
			wprintf( sString, pMercSheLeaveString[ 4 ], pSoldier->name, zShortTownIDString );
		}
		fInSector = TRUE;
	}

	// check if drassen controlled
	else if( StrategicMap[  ( AIRPORT_X + ( MAP_WORLD_X * AIRPORT_Y ) ) ].fEnemyControlled == FALSE )
	{
		
		if( ( pSoldier->sSectorX == AIRPORT_X ) && ( pSoldier->sSectorY == AIRPORT_Y ) && ( pSoldier->bSectorZ == 0 ) )
		{
			if( gMercProfiles[ pSoldier->ubProfile ].bSex == MALE )
			{
				wprintf( sString, L"%s %s", pSoldier->name, pMercHeLeaveString[ 3 ] );
			}
			else
			{
				wprintf( sString, L"%s %s", pSoldier->name, pMercSheLeaveString[ 3 ] );
			}
			fInSector = TRUE;
		}
		else
		{
			// Set string for generic button
			wprintf( gzUserDefinedButton2, L"B13" );

			if( gMercProfiles[ pSoldier->ubProfile ].bSex == MALE )
			{
				wprintf( sString, pMercHeLeaveString[ 0 ], pSoldier->name, zShortTownIDString );
			}
			else
			{
				wprintf( sString, pMercSheLeaveString[ 0 ], pSoldier->name, zShortTownIDString );
			}
			
		}
	}
	else
	{
		if( ( pSoldier->sSectorX == OMERTA_LEAVE_EQUIP_SECTOR_X ) && ( pSoldier->sSectorY == OMERTA_LEAVE_EQUIP_SECTOR_Y ) && ( pSoldier->bSectorZ == 0 ) )
		{
			if( gMercProfiles[ pSoldier->ubProfile ].bSex == MALE )
			{
				wprintf( sString, L"%s %s", pSoldier->name, pMercHeLeaveString[ 2 ] );
			}
			else
			{
				wprintf( sString, L"%s %s", pSoldier->name, pMercSheLeaveString[ 2 ] );
			}
			fInSector = TRUE;
		}
		else
		{
			// Set string for generic button
			wprintf( gzUserDefinedButton2, L"A9" );

			if( gMercProfiles[ pSoldier->ubProfile ].bSex == MALE )
			{
				wprintf( sString, pMercHeLeaveString[ 1 ], pSoldier->name, zShortTownIDString );
			}
			else
			{
				wprintf( sString, pMercSheLeaveString[ 1 ], pSoldier->name, zShortTownIDString );
			}
		}
	}

	/// which screen are we in?
	if ( (guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) )
	{
		if( fInSector == FALSE )
		{			
			// set up for mapscreen
			DoMapMessageBox( MSG_BOX_BASIC_STYLE, sString, MAP_SCREEN, ( UINT16 )( ( fAddRehireButton ? MSG_BOX_FLAG_GENERICCONTRACT : MSG_BOX_FLAG_GENERIC ) ), MercDepartEquipmentBoxCallBack );
		}
		else
		{
			DoMapMessageBox( MSG_BOX_BASIC_STYLE, sString, MAP_SCREEN, ( UINT16 )( ( fAddRehireButton ? MSG_BOX_FLAG_OKCONTRACT : MSG_BOX_FLAG_OK  ) ), MercDepartEquipmentBoxCallBack );
		}

	}
	else
	{
		if( fInSector == FALSE )
		{
			// set up for all otherscreens
			DoMessageBox(  MSG_BOX_BASIC_STYLE, sString,  guiCurrentScreen, ( UINT16 ) ( MSG_BOX_FLAG_USE_CENTERING_RECT | ( fAddRehireButton ? MSG_BOX_FLAG_GENERICCONTRACT : MSG_BOX_FLAG_GENERIC ) ),  MercDepartEquipmentBoxCallBack,  &pCenteringRect );
		}
		else
		{
			DoMessageBox(  MSG_BOX_BASIC_STYLE, sString,  guiCurrentScreen, ( UINT16 ) ( MSG_BOX_FLAG_USE_CENTERING_RECT | ( fAddRehireButton ? MSG_BOX_FLAG_OKCONTRACT : MSG_BOX_FLAG_OK  ) ) ,  MercDepartEquipmentBoxCallBack,  &pCenteringRect );
		}
	}
	
	if( pSoldier->fSignedAnotherContract == TRUE )
	{
		//fCurrentMercFired = FALSE;
	}

}

void MercDepartEquipmentBoxCallBack( UINT8 bExitValue )
{
	// gear left in current sector?
	if( pLeaveSoldier == NULL )
	{
		return;
	}

	if( bExitValue == MSG_BOX_RETURN_OK )
	{
		// yep (NOTE that this passes the SOLDIER index, not the PROFILE index as the others do)
		HandleLeavingOfEquipmentInCurrentSector( pLeaveSoldier->ubID );
		
		// aim merc will say goodbye when leaving
		if( ( pLeaveSoldier->ubWhatKindOfMercAmI == MERC_TYPE__AIM_MERC ) && ( ubQuitType != HISTORY_MERC_FIRED ) )
		{
		//	TacticalCharacterDialogue( pLeaveSoldier, QUOTE_MERC_LEAVING_ALSUCO_SOON );
		}
	}
	else if( bExitValue == MSG_BOX_RETURN_CONTRACT )
	{
		HandleExtendMercsContract( pLeaveSoldier );
		return;
	}
  else if( bExitValue == MSG_BOX_RETURN_YES )
	{
		// yep (NOTE that this passes the SOLDIER index, not the PROFILE index as the others do)
		HandleLeavingOfEquipmentInCurrentSector( pLeaveSoldier->ubID );

		// aim merc will say goodbye when leaving
		if( ( pLeaveSoldier->ubWhatKindOfMercAmI == MERC_TYPE__AIM_MERC ) && ( ubQuitType != HISTORY_MERC_FIRED ) )
		{
		//	TacticalCharacterDialogue( pLeaveSoldier, QUOTE_MERC_LEAVING_ALSUCO_SOON );
		}
	}
	else
	{
		// no
		if( StrategicMap[ BOBBYR_SHIPPING_DEST_SECTOR_X + ( BOBBYR_SHIPPING_DEST_SECTOR_Y * MAP_WORLD_X ) ].fEnemyControlled == FALSE )
		{
			HandleMercLeavingEquipmentInDrassen( pLeaveSoldier->ubID );
		}
		else
		{
			HandleMercLeavingEquipmentInOmerta( pLeaveSoldier->ubID );
		}
	}


	StrategicRemoveMerc( pLeaveSoldier );

	pLeaveSoldier = NULL;

	return;
}



BOOLEAN HandleFiredDeadMerc( SOLDIERTYPE *pSoldier )
{
	AddCharacterToDeadList( pSoldier );

#if 0
	//if the dead merc is in the current sector
	if( pSoldier->sSectorX == gWorldSectorX &&
			pSoldier->sSectorY == gWorldSectorY &&
			pSoldier->bSectorZ == gbWorldSectorZ )
	{
		TurnSoldierIntoCorpse( pSoldier, FALSE, FALSE );
	}
	else
	{
		ROTTING_CORPSE_DEFINITION		Corpse;

		// Setup some values!
		Corpse.ubBodyType							= pSoldier->ubBodyType;
		Corpse.sGridNo								= pSoldier->sInsertionGridNo;
		Corpse.dXPos									= pSoldier->dXPos;
		Corpse.dYPos									= pSoldier->dYPos;
		Corpse.sHeightAdjustment			= pSoldier->sHeightAdjustment;

		SET_PALETTEREP_ID ( Corpse.HeadPal,		pSoldier->HeadPal );
		SET_PALETTEREP_ID ( Corpse.VestPal,		pSoldier->VestPal );
		SET_PALETTEREP_ID ( Corpse.SkinPal,		pSoldier->SkinPal );
		SET_PALETTEREP_ID ( Corpse.PantsPal,   pSoldier->PantsPal );

		Corpse.bDirection	= pSoldier->bDirection;

		// Set time of death
		Corpse.uiTimeOfDeath = GetWorldTotalMin( );

		// Set type
		Corpse.ubType	= (UINT8)gubAnimSurfaceCorpseID[ pSoldier->ubBodyType][ pSoldier->usAnimState ];

		//Add the rotting corpse info to the sectors unloaded rotting corpse file
		AddRottingCorpseToUnloadedSectorsRottingCorpseFile( pSoldier->sSectorX, pSoldier->sSectorY, pSoldier->bSectorZ, &Corpse);
	}
#endif

	return( TRUE );
}


void HandleExtendMercsContract( SOLDIERTYPE *pSoldier )
{
	if ( !(guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) )
	{
		gfEnteringMapScreen = TRUE;

		fEnterMapDueToContract = TRUE;
		pContractReHireSoldier = pSoldier;
		LeaveTacticalScreen( MAP_SCREEN );
		uiContractTimeMode = TIME_COMPRESS_5MINS;
	}
	else
	{
		FindAndSetThisContractSoldier( pSoldier );
		pContractReHireSoldier = pSoldier;
		uiContractTimeMode = giTimeCompressMode;
	}
	
	fTeamPanelDirty = TRUE;
	fCharacterInfoPanelDirty = TRUE;

	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE, 1, MAP_SCREEN, 0, 0, 0 );
	
	CheckIfSalaryIncreasedAndSayQuote( pSoldier, TRUE );

	SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE, 0 ,MAP_SCREEN, 0, 0, 0 );

	return;
}



void FindOutIfAnyMercAboutToLeaveIsGonnaRenew( void )
{
	// find out is something was said
	SOLDIERTYPE *pSoldier = NULL, *pSoldierWhoWillQuit = NULL;
	INT32				iCounter= 0, iNumberOnTeam = 0;
	UINT8				ubPotentialMercs[ 20 ] = { 0 };
	UINT8				ubNumMercs = 0;
	UINT8				ubChosenMerc;

	gfFirstMercSayQuote = FALSE;

	pSoldier = &Menptr[ 0 ];
	iNumberOnTeam =gTacticalStatus.Team[ OUR_TEAM ].bLastID;

	// run through list of grunts whoose contract are up in the next 2 hours
	// ATE: AND - build list THEN choose one!
	// What we will do here is make a list of mercs that will want
	// to stay if offered. Durning that process, also check if there
	// is any merc that does not want to stay and only display that quote
	// if they are the only one here....
	for( iCounter = 0; iCounter < iNumberOnTeam; iCounter++ )
	{
		pSoldier = &Menptr[ iCounter ];
		
		// valid soldier?
		if( ( pSoldier->bActive == FALSE ) || ( pSoldier->bLife == 0 ) || ( pSoldier->bAssignment == IN_TRANSIT ) ||( pSoldier->bAssignment == ASSIGNMENT_POW ) )
		{
			// no
			continue;
		}

		if( pSoldier->ubWhatKindOfMercAmI == MERC_TYPE__AIM_MERC )
		{
			//if the user hasnt renewed yet, and is still leaving today
			if ( ContractIsGoingToExpireSoon( pSoldier ) )
			{
				// OK, default value for quote said
				pSoldier->ubContractRenewalQuoteCode = SOLDIER_CONTRACT_RENEW_QUOTE_NOT_USED;

				// Add this guy to the renewal list
				ContractRenewalList[ ubNumContractRenewals ].ubProfileID = pSoldier->ubProfile;
				ubNumContractRenewals++;

				if( WillMercRenew( pSoldier, FALSE ) )
				{
					ubPotentialMercs[ ubNumMercs ] = pSoldier->ubID;
					ubNumMercs++;
				}
				else
				{
					pSoldierWhoWillQuit = pSoldier;
				}

				// Add to list!
				AddSoldierToWaitingListQueue( pSoldier );

			}
		}
		else
		{
			if( pSoldier->ubWhatKindOfMercAmI == MERC_TYPE__MERC )
			{
				// Do nothing here for now...
			}
		}
	}



	// OK, check if we should display line for the guy who does not want
	// to stay
	if ( ubNumMercs == 0 && pSoldierWhoWillQuit != NULL )
	{
		// OK, he does not want to renew.......
		HandleImportantMercQuote( pSoldierWhoWillQuit, QUOTE_MERC_LEAVING_ALSUCO_SOON );

		AddReasonToWaitingListQueue( CONTRACT_EXPIRE_WARNING_REASON );		
		TacticalCharacterDialogueWithSpecialEvent( pSoldierWhoWillQuit, 0, DIALOGUE_SPECIAL_EVENT_SHOW_UPDATE_MENU, 0,0 );

		pSoldierWhoWillQuit->ubContractRenewalQuoteCode = SOLDIER_CONTRACT_RENEW_QUOTE_115_USED;

	}
	else
	{
		// OK, pick one....
		if ( ubNumMercs > 0 )
		{
			ubChosenMerc = (UINT8)Random( ubNumMercs );

			SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,1 ,MAP_SCREEN ,0 ,0 ,0 );
			HandleImportantMercQuote( MercPtrs[ ubPotentialMercs[ ubChosenMerc ] ], QUOTE_CONTRACTS_OVER );
			SpecialCharacterDialogueEvent( DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE,0 ,MAP_SCREEN ,0 ,0 ,0 );

			AddReasonToWaitingListQueue( CONTRACT_EXPIRE_WARNING_REASON );		
			TacticalCharacterDialogueWithSpecialEvent( MercPtrs[ ubPotentialMercs[ ubChosenMerc ] ], 0, DIALOGUE_SPECIAL_EVENT_SHOW_UPDATE_MENU, 0,0 );

			MercPtrs[ ubPotentialMercs[ ubChosenMerc ] ]->ubContractRenewalQuoteCode = SOLDIER_CONTRACT_RENEW_QUOTE_89_USED;
		}
	}
}

void HandleNotifyPlayerCantAffordInsurance( void )
{
	DoScreenIndependantMessageBox( zMarksMapScreenText[ 9 ], MSG_BOX_FLAG_OK, NULL );
}


void HandleNotifyPlayerCanAffordInsurance( SOLDIERTYPE *pSoldier, UINT8 ubLength, INT32 iCost )
{
	CHAR16 sString[ 128 ];
	CHAR16 sStringA[ 32 ];

	//parse the cost
	wprintf( sStringA, L"%d",iCost );

	// insert the commans and dollar sign
	InsertCommasForDollarFigure( sStringA );
	InsertDollarSignInToString( sStringA );

	wprintf( sString, zMarksMapScreenText[ 10 ], pSoldier->name, sStringA, ubLength );

	//Set the length to the global variable ( so we know how long the contract is in the callback )
	gubContractLength = ubLength;
	gpInsuranceSoldier = pSoldier;

	//Remember the soldier aswell
	pContractReHireSoldier = pSoldier;

	// now pop up the message box
	DoScreenIndependantMessageBox( sString, MSG_BOX_FLAG_YESNO, ExtendMercInsuranceContractCallBack );
	
	return;
}

void ExtendMercInsuranceContractCallBack( UINT8 bExitValue )
{
  if( bExitValue == MSG_BOX_RETURN_YES )
	{
		PurchaseOrExtendInsuranceForSoldier( gpInsuranceSoldier, gubContractLength );
	}

	// OK, handle ending of renew session
	if ( gfInContractMenuFromRenewSequence )
	{
		EndCurrentContractRenewal( );
	}

	gpInsuranceSoldier = NULL;
}

void HandleUniqueEventWhenPlayerLeavesTeam( SOLDIERTYPE *pSoldier )
{
	switch( pSoldier->ubProfile )
	{
		//When iggy leaves the players team,
		case IGGY:
			//if he is owed money ( ie the player didnt pay him )
			if( gMercProfiles[ pSoldier->ubProfile ].iBalance < 0 )
			{
				//iggy is now available to be handled by the enemy
				gubFact[ FACT_IGGY_AVAILABLE_TO_ARMY ] = TRUE;
			}
		break;
	}
}


UINT32 GetHourWhenContractDone( SOLDIERTYPE *pSoldier )
{
	UINT32	uiArriveHour;

	// Get the arrival hour - that will give us when they arrived....
	uiArriveHour = ( ( pSoldier->uiTimeSoldierWillArrive ) - ( ( ( pSoldier->uiTimeSoldierWillArrive ) / 1440 ) * 1440 ) ) / 60;

	return( uiArriveHour );
}


BOOLEAN ContractIsExpiring( SOLDIERTYPE *pSoldier )
{
	UINT32	uiCheckHour;

	// First at least make sure same day....
	if( ( pSoldier->iEndofContractTime /1440 ) <= (INT32)GetWorldDay( ) )
	{
		uiCheckHour = GetHourWhenContractDone( pSoldier );

		// See if the hour we are on is the same....
		if ( GetWorldHour( ) == uiCheckHour )
		{
			// All's good for go!
			return( TRUE );			
		}
	}

	return( FALSE );
}


BOOLEAN ContractIsGoingToExpireSoon( SOLDIERTYPE *pSoldier )
{
	// get hour contract is going to expire....
	UINT32 uiCheckHour;

	// First at least make sure same day....
	if( ( pSoldier->iEndofContractTime /1440 ) <= (INT32)GetWorldDay( ) )
	{
		uiCheckHour = GetHourWhenContractDone( pSoldier );

		// If we are <= 2 hours from expiry.
		if ( GetWorldHour( ) >= ( uiCheckHour - 2 ) )
		{
			// All's good for go!
			return( TRUE );			
		}
	}

	return( FALSE );

}