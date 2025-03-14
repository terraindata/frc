//
// Created by ctripp on 1/25/25.
//

#include <xrcpp/impl/pointer_bases.h>
#include <xrcpp/impl/xrc_settings.h>
#include <xrcpp/impl/concurrent_pointer_base.h>
#include <xrcpp/impl/shared_pointer_base.h>
#include <xrcpp/impl/AtomicPointerBase.h>
#include <xrcpp/impl/controlled_type_layout.h>
#include <xrcpp/impl/managed_type_registry.h>
#include <xrcpp/impl/public_pointer_base.h>


int main()
{
    xrc::impl::concurrent_pointer_base a;
    xrc::impl::shared_pointer_base     b;

    return 0;
}
