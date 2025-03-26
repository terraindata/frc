//
// Created by ctripp on 3/3/25.
//

#pragma once


namespace xrcpp::impl
{
class xrc_pointer
{
};
}

//namespace std
//{
//    /**
//     * std lib specialization of std::hash for shared_pointer_base
//     */
//    template<>
//    struct hash<xrcpp::impl::xrc_pointer>
//    {
//        template<class T, typename = std::enable_if_t<std::is_base_of_v<xrcpp::impl::xrc_pointer, T> > >
//        std::size_t operator()(const T& target) const
//        {
//            return hash<void*>()(target.get());
//        }
//    };
//}
