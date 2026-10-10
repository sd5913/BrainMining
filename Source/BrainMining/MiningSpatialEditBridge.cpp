#include "MiningSpatialEditBridge.h"

#if WITH_EDITOR
MiningSpatialEditing::FEditHandler MiningSpatialEditing::Handler;
bool MiningSpatialEditing::Handle(AActor* Actor) { return Handler.IsBound() && Handler.Execute(Actor); }
#endif
