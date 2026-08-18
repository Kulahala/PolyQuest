#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Animation/AnimMontage.h"
#include "Combat/Enemy/EnemyAttackProfile.h"
#include "Combat/Enemy/EnemyAttackSet.h"
#include "GameplayEffect.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEnemyAttackSetSelectionTest,
	"PolyQuest.Enemy.AttackSetSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyAttackSetSelectionTest::RunTest(const FString& Parameters)
{
	// Create dummy test objects for valid profile construction
	UAnimMontage* DummyMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_DummyMontage"));
	UClass* DummyGEClass = UGameplayEffect::StaticClass();

	// Helper to create a valid test attack profile
	auto CreateValidProfile = [&](const TCHAR* Name, float Range, float Cooldown, float GuardDamage) -> UEnemyAttackProfile*
	{
		UEnemyAttackProfile* Profile = NewObject<UEnemyAttackProfile>(GetTransientPackage(), Name);
		Profile->SetTestMontage(DummyMontage);
		Profile->SetTestDamageEffectClass(DummyGEClass);
		Profile->SetTestAttackRange(Range);
		Profile->SetTestCooldown(Cooldown);
		Profile->SetTestGuardStaminaDamage(GuardDamage);
		return Profile;
	};

	UEnemyAttackProfile* ProfileA = CreateValidProfile(TEXT("Test_ProfileA"), 200.0f, 1.0f, 20.0f);
	UEnemyAttackProfile* ProfileB = CreateValidProfile(TEXT("Test_ProfileB"), 250.0f, 1.5f, 30.0f);
	UEnemyAttackProfile* ProfileShort = CreateValidProfile(TEXT("Test_ProfileShort"), 120.0f, 0.8f, 15.0f);

	TestTrue(TEXT("ProfileA is valid"), ProfileA->IsValidAttackProfile());
	TestTrue(TEXT("ProfileB is valid"), ProfileB->IsValidAttackProfile());
	TestTrue(TEXT("ProfileShort is valid"), ProfileShort->IsValidAttackProfile());

	// 0. Profile Validation Finite Checks
	{
		UEnemyAttackProfile* FiniteTestProfile = CreateValidProfile(TEXT("Test_FiniteProfile"), 200.0f, 1.0f, 20.0f);
		TestTrue(TEXT("Baseline FiniteTestProfile is valid"), FiniteTestProfile->IsValidAttackProfile());

		// AttackRange finite checks
		FiniteTestProfile->SetTestAttackRange(std::numeric_limits<float>::infinity());
		TestFalse(TEXT("Profile with +INF AttackRange is rejected"), FiniteTestProfile->IsValidAttackProfile());
		FiniteTestProfile->SetTestAttackRange(std::numeric_limits<float>::quiet_NaN());
		TestFalse(TEXT("Profile with NaN AttackRange is rejected"), FiniteTestProfile->IsValidAttackProfile());
		FiniteTestProfile->SetTestAttackRange(200.0f);

		// CooldownAfterAttack finite checks
		FiniteTestProfile->SetTestCooldown(std::numeric_limits<float>::infinity());
		TestFalse(TEXT("Profile with +INF CooldownAfterAttack is rejected"), FiniteTestProfile->IsValidAttackProfile());
		FiniteTestProfile->SetTestCooldown(std::numeric_limits<float>::quiet_NaN());
		TestFalse(TEXT("Profile with NaN CooldownAfterAttack is rejected"), FiniteTestProfile->IsValidAttackProfile());
		FiniteTestProfile->SetTestCooldown(1.0f);

		// GuardStaminaDamage finite checks
		FiniteTestProfile->SetTestGuardStaminaDamage(std::numeric_limits<float>::infinity());
		TestFalse(TEXT("Profile with +INF GuardStaminaDamage is rejected"), FiniteTestProfile->IsValidAttackProfile());
		FiniteTestProfile->SetTestGuardStaminaDamage(std::numeric_limits<float>::quiet_NaN());
		TestFalse(TEXT("Profile with NaN GuardStaminaDamage is rejected"), FiniteTestProfile->IsValidAttackProfile());
		FiniteTestProfile->SetTestGuardStaminaDamage(20.0f);
	}

	// 1. Attack Set Validation Tests
	{
		UEnemyAttackSet* TestSet = NewObject<UEnemyAttackSet>(GetTransientPackage(), TEXT("Test_AttackSet_Validation"));
		FString Reason;

		// 1.1 Empty set must be rejected
		TestSet->ClearTestEntries();
		TestSet->SetTestEngagementRange(200.0f);
		TestFalse(TEXT("Empty AttackSet is rejected"), TestSet->IsAttackSetValid(Reason));
		TestTrue(TEXT("Empty reason describes empty list"), Reason.Contains(TEXT("empty Entries list")));

		// 1.2 Non-positive / non-finite EngagementRange must be rejected
		TestSet->AddTestEntry(ProfileA, 1.0f);
		TestSet->SetTestEngagementRange(0.0f);
		TestFalse(TEXT("Zero EngagementRange is rejected"), TestSet->IsAttackSetValid(Reason));
		TestSet->SetTestEngagementRange(-50.0f);
		TestFalse(TEXT("Negative EngagementRange is rejected"), TestSet->IsAttackSetValid(Reason));
		TestSet->SetTestEngagementRange(std::numeric_limits<float>::infinity());
		TestFalse(TEXT("+INF EngagementRange is rejected"), TestSet->IsAttackSetValid(Reason));
		TestSet->SetTestEngagementRange(std::numeric_limits<float>::quiet_NaN());
		TestFalse(TEXT("NaN EngagementRange is rejected"), TestSet->IsAttackSetValid(Reason));

		// 1.3 Null profile entry must be rejected
		TestSet->SetTestEngagementRange(200.0f);
		TestSet->ClearTestEntries();
		TestSet->AddTestEntry(nullptr, 1.0f);
		TestFalse(TEXT("Null AttackProfile entry is rejected"), TestSet->IsAttackSetValid(Reason));
		TestTrue(TEXT("Null profile reason names entry"), Reason.Contains(TEXT("null AttackProfile")));

		// 1.4 Invalid profile entry must be rejected
		UEnemyAttackProfile* InvalidProfile = NewObject<UEnemyAttackProfile>(GetTransientPackage(), TEXT("Test_InvalidProfile"));
		TestSet->ClearTestEntries();
		TestSet->AddTestEntry(InvalidProfile, 1.0f);
		TestFalse(TEXT("Invalid AttackProfile entry is rejected"), TestSet->IsAttackSetValid(Reason));
		TestTrue(TEXT("Invalid profile reason names invalid profile"), Reason.Contains(TEXT("references an invalid AttackProfile")));

		// 1.5 Non-positive / non-finite selection weight must be rejected
		TestSet->ClearTestEntries();
		TestSet->AddTestEntry(ProfileA, 0.0f);
		TestFalse(TEXT("Zero selection weight is rejected"), TestSet->IsAttackSetValid(Reason));
		TestSet->ClearTestEntries();
		TestSet->AddTestEntry(ProfileA, -2.0f);
		TestFalse(TEXT("Negative selection weight is rejected"), TestSet->IsAttackSetValid(Reason));
		TestSet->ClearTestEntries();
		TestSet->AddTestEntry(ProfileA, std::numeric_limits<float>::infinity());
		TestFalse(TEXT("+INF selection weight is rejected"), TestSet->IsAttackSetValid(Reason));
		TestSet->ClearTestEntries();
		TestSet->AddTestEntry(ProfileA, std::numeric_limits<float>::quiet_NaN());
		TestFalse(TEXT("NaN selection weight is rejected"), TestSet->IsAttackSetValid(Reason));

		// 1.6 Duplicate profile reference must be rejected
		TestSet->ClearTestEntries();
		TestSet->AddTestEntry(ProfileA, 1.0f);
		TestSet->AddTestEntry(ProfileA, 2.0f);
		TestFalse(TEXT("Duplicate AttackProfile reference is rejected"), TestSet->IsAttackSetValid(Reason));
		TestTrue(TEXT("Duplicate reason names duplicate profile"), Reason.Contains(TEXT("contains duplicate AttackProfile")));

		// 1.7 EngagementRange that no entry can reach must be rejected
		TestSet->ClearTestEntries();
		TestSet->SetTestEngagementRange(200.0f);
		TestSet->AddTestEntry(ProfileShort, 1.0f); // Short has Range 120, EngagementRange is 200
		TestFalse(TEXT("EngagementRange exceeding all entries' AttackRange is rejected"), TestSet->IsAttackSetValid(Reason));
		TestTrue(TEXT("Unreachable engagement range reason matches"), Reason.Contains(TEXT("no AttackProfile whose AttackRange reaches EngagementRange")));

		// 1.8 Total weight overflow check
		TestSet->ClearTestEntries();
		TestSet->SetTestEngagementRange(200.0f);
		TestSet->AddTestEntry(ProfileA, std::numeric_limits<float>::max());
		TestSet->AddTestEntry(ProfileB, std::numeric_limits<float>::max()); // sum overflows to +INF
		TestFalse(TEXT("Overflowing total weight sum is rejected"), TestSet->IsAttackSetValid(Reason));

		// 1.9 Single valid entry is accepted
		TestSet->ClearTestEntries();
		TestSet->SetTestEngagementRange(180.0f);
		TestSet->AddTestEntry(ProfileA, 1.0f); // ProfileA Range 200 >= 180
		TestTrue(TEXT("Single valid entry is accepted"), TestSet->IsAttackSetValid(Reason));

		// 1.10 Multiple valid entries are accepted
		TestSet->ClearTestEntries();
		TestSet->SetTestEngagementRange(200.0f);
		TestSet->AddTestEntry(ProfileA, 1.0f); // Range 200
		TestSet->AddTestEntry(ProfileB, 3.0f); // Range 250
		TestSet->AddTestEntry(ProfileShort, 2.0f); // Range 120 (< 200, but ProfileA and ProfileB reach 200)
		TestTrue(TEXT("Multiple valid entries with reachable range accepted"), TestSet->IsAttackSetValid(Reason));
	}

	// 2. Pure Weighted Selection Tests
	{
		UEnemyAttackSet* Set = NewObject<UEnemyAttackSet>(GetTransientPackage(), TEXT("Test_AttackSet_Selection"));
		Set->SetTestEngagementRange(200.0f);
		Set->AddTestEntry(ProfileA, 1.0f); // Range 200, Weight 1.0
		Set->AddTestEntry(ProfileB, 3.0f); // Range 250, Weight 3.0

		FString Reason;
		TestTrue(TEXT("Selection test set is valid"), Set->IsAttackSetValid(Reason));

		// 2.1 Out of bounds target distance
		TestNull(TEXT("Target distance > EngagementRange returns nullptr"), Set->SelectAttackProfile(210.0f, 0.5f));
		TestNull(TEXT("Negative target distance returns nullptr"), Set->SelectAttackProfile(-10.0f, 0.5f));
		TestNull(TEXT("+INF target distance returns nullptr"), Set->SelectAttackProfile(std::numeric_limits<float>::infinity(), 0.5f));
		TestNull(TEXT("NaN target distance returns nullptr"), Set->SelectAttackProfile(std::numeric_limits<float>::quiet_NaN(), 0.5f));

		// 2.2 Invalid random fraction
		TestNull(TEXT("Negative random fraction returns nullptr"), Set->SelectAttackProfile(150.0f, -0.1f));
		TestNull(TEXT("Random fraction > 1.0 returns nullptr"), Set->SelectAttackProfile(150.0f, 1.1f));
		TestNull(TEXT("+INF random fraction returns nullptr"), Set->SelectAttackProfile(150.0f, std::numeric_limits<float>::infinity()));
		TestNull(TEXT("NaN random fraction returns nullptr"), Set->SelectAttackProfile(150.0f, std::numeric_limits<float>::quiet_NaN()));

		// 2.3 Deterministic weighted selection at distance 150 (both ProfileA and ProfileB eligible)
		// Total weight = 1.0 + 3.0 = 4.0
		// Threshold: Fraction * 4.0
		// Fraction 0.00 -> Target 0.00 <= 1.00 -> ProfileA
		// Fraction 0.25 -> Target 1.00 <= 1.00 -> ProfileA
		// Fraction 0.26 -> Target 1.04 > 1.00, <= 4.00 -> ProfileB
		// Fraction 0.75 -> Target 3.00 > 1.00, <= 4.00 -> ProfileB
		// Fraction 1.00 -> Target 4.00 <= 4.00 -> ProfileB
		TestEqual(TEXT("Fraction 0.0 selects ProfileA"), Set->SelectAttackProfile(150.0f, 0.0f), Cast<const UEnemyAttackProfile>(ProfileA));
		TestEqual(TEXT("Fraction 0.25 selects ProfileA (boundary)"), Set->SelectAttackProfile(150.0f, 0.25f), Cast<const UEnemyAttackProfile>(ProfileA));
		TestEqual(TEXT("Fraction 0.26 selects ProfileB"), Set->SelectAttackProfile(150.0f, 0.26f), Cast<const UEnemyAttackProfile>(ProfileB));
		TestEqual(TEXT("Fraction 0.75 selects ProfileB"), Set->SelectAttackProfile(150.0f, 0.75f), Cast<const UEnemyAttackProfile>(ProfileB));
		TestEqual(TEXT("Fraction 1.0 selects ProfileB (upper bound)"), Set->SelectAttackProfile(150.0f, 1.0f), Cast<const UEnemyAttackProfile>(ProfileB));

		// 2.4 Range filtering: At distance 205 (ProfileA Range 200 is filtered out, only ProfileB Range 250 is eligible)
		// Total weight = 3.0
		// All valid fractions [0, 1] must select ProfileB
		Set->SetTestEngagementRange(250.0f);
		TestEqual(TEXT("At distance 205, Fraction 0.0 selects ProfileB"), Set->SelectAttackProfile(205.0f, 0.0f), Cast<const UEnemyAttackProfile>(ProfileB));
		TestEqual(TEXT("At distance 205, Fraction 0.5 selects ProfileB"), Set->SelectAttackProfile(205.0f, 0.5f), Cast<const UEnemyAttackProfile>(ProfileB));
		TestEqual(TEXT("At distance 205, Fraction 1.0 selects ProfileB"), Set->SelectAttackProfile(205.0f, 1.0f), Cast<const UEnemyAttackProfile>(ProfileB));

		// 2.5 Distance where all entries are filtered out
		UEnemyAttackSet* ShortSet = NewObject<UEnemyAttackSet>(GetTransientPackage(), TEXT("Test_ShortSet"));
		ShortSet->SetTestEngagementRange(100.0f);
		ShortSet->AddTestEntry(ProfileShort, 1.0f); // Range 120
		// Target at 150 > EngagementRange (100) -> null
		TestNull(TEXT("Target distance 150 > EngagementRange 100 returns null"), ShortSet->SelectAttackProfile(150.0f, 0.5f));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
