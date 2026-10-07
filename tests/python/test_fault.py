import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "python"))
from kyty import FaultManager, FaultKind, FaultAction, DeadlockClass, FaultRecord

def test_bounded_retry():
    fm = FaultManager()
    for i in range(FaultManager.MAX_RETRIES + 2):
        a = fm.record(FaultRecord(kind=FaultKind.BdaFault, guest_address=0x10000,
                                  retry_count=i, action=FaultAction.Resolved))
        if i >= FaultManager.MAX_RETRIES:
            assert a == FaultAction.ControlledFailure
    assert len(fm.records) == FaultManager.MAX_RETRIES + 2
    assert fm.classify() != DeadlockClass.None_
    fm.clear()
    assert fm.classify() == DeadlockClass.None_
