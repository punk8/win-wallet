// xdagnetwalletCLI.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <msclr\marshal_cppstd.h>

#include "XDagRuntime.h"
#include "XDagException.h"

#pragma unmanaged

#include "xdag_runtime.h"


#pragma managed


using namespace System;
using namespace System::Runtime::InteropServices;
using namespace XDagNetWalletCLI;


XDagRuntime::XDagRuntime(IXDagWallet^ wallet)
{
	if (wallet == nullptr)
	{
		throw gcnew System::ArgumentNullException();
	}

	this->xdagWallet = wallet;

	// Set Logger Callback
	InputPasswordDelegate^ func = gcnew InputPasswordDelegate(this, &XDagRuntime::InputPassword);
	gch = GCHandle::Alloc(func);
	IntPtr funcPtr = Marshal::GetFunctionPointerForDelegate(func);
	InputPasswordStd necb = static_cast<InputPasswordStd>(funcPtr.ToPointer());
	xdag_set_password_callback_wrap(necb);

	ShowStateDelegate^ func2 = gcnew ShowStateDelegate(this, &XDagRuntime::OnUpdateState);
	gch = GCHandle::Alloc(func2);
	IntPtr funcPtr2 = Marshal::GetFunctionPointerForDelegate(func2);
	//// g_xdag_show_state = static_cast<ShowStateStd>(funcPtr2.ToPointer());

	
}

XDagRuntime::~XDagRuntime()
{
	Marshal::FreeHGlobal(pooAddressPtr);

}

void XDagRuntime::Start(String^ poolAddress, bool isTestnet)
{
	std::string exeFile = "xdag.exe";
	//// std::string poolAddressStd = msclr::interop::marshal_as<std::string>(poolAddress);

	pooAddressPtr = Marshal::StringToHGlobalAnsi(poolAddress);
	const char* poolAddressStd = (const char*)(void*)pooAddressPtr;

	// std::string poolAddress = "feipool.xyz:13654";

	EventCallbackDelegate^ func3 = gcnew EventCallbackDelegate(this, &XDagRuntime::EventCallback);
	gch = GCHandle::Alloc(func3);
	IntPtr funcPtr3 = Marshal::GetFunctionPointerForDelegate(func3);
	EventCallbackStd necb3 = static_cast<EventCallbackStd>(funcPtr3.ToPointer());
	xdag_set_event_callback_wrap(necb3);

	char *argv[] = { (char*)exeFile.c_str() };
	int result = xdag_init_wrap(1, argv, poolAddressStd, isTestnet);

	if (result == 0)
	{
		return;
	}

	switch (result)
	{
	case -1:
		throw gcnew ArgumentNullException("Password is Empty");
		break;
	case -2:
	case -3:
		throw gcnew PasswordIncorrectException();
		break;
	default:
		break;
	}
}

bool XDagRuntime::HasExistingAccount()
{
	return xdag_dnet_crpt_found();
}

bool XDagRuntime::ValidateWalletAddress(String^ address)
{
	std::string addressStd = msclr::interop::marshal_as<std::string>(address);

	return xdag_is_valid_wallet_address(addressStd.c_str());
}

bool XDagRuntime::ValidateRemark(String^ remark)
{
	std::string remarkStd = msclr::interop::marshal_as<std::string>(remark);

	return xdag_is_valid_remark(remarkStd.c_str());
}

void XDagRuntime::TransferToAddress(String^ toAddress, double amount, String^ remark)
{
	std::string addressStd = msclr::interop::marshal_as<std::string>(toAddress);
	std::string amountStd = msclr::interop::marshal_as<std::string>(amount.ToString());
	std::string remarkStd = msclr::interop::marshal_as<std::string>(remark);


	int result = xdag_transfer_wrap(addressStd.c_str(), amountStd.c_str(), remarkStd.c_str());

	if (result == 0)
	{
		// Success
		return;
	}

	switch (result)
	{
	case -3:
	case -4:
		throw gcnew InsufficientAmountException();
	case -5:
		throw gcnew WalletAddressFormatException();
	case -6:
		throw gcnew PasswordIncorrectException();
	case error_pwd_incorrect:
		throw gcnew PasswordIncorrectException();
	default:
		throw gcnew InvalidOperationException("Failed to commit transfer. ErrorCode=[" + result.ToString() + "]");
		break;
	}

}

void XDagRuntime::DoTesting()
{
}

int XDagRuntime::InputPassword(const char *prompt, char *buf, unsigned size)
{
	if (this->xdagWallet == nullptr)
	{
		return -1;
	}

	String ^ promptString = ConvertFromConstChar(prompt);

	String ^ passwordString = this->xdagWallet->OnPromptInputPassword(promptString, (UINT)size);
	std::string passwordStd = msclr::interop::marshal_as<std::string>(passwordString);

	const char* passwordChars = passwordStd.c_str();
	if (strlen(passwordChars) == 0)
	{
		return -1;
	}

	strncpy_s(buf, size, passwordChars, size);

	return 0;
}

int XDagRuntime::OnUpdateState(const char *state, const char *balance, const char *address)
{
	String^ stateString = ConvertFromConstChar(state);
	String^ balanceString = ConvertFromConstChar(balance);
	String^ addressString = ConvertFromConstChar(address);

	//// this->xdagWallet->OnUpdateState(stateString, balanceString, addressString, String::Empty);

	return 0;
}

int XDagRuntime::EventCallback(void* obj, xdag_event * eve)
{
	int eventId = eve->event_id;
	String^ eventData = ConvertFromConstChar(eve->event_data);

	this->xdagWallet->OnUpdateState(eventId.ToString(), String::Empty, String::Empty, eventData);

	switch (eventId)
	{
	case event_id_log:
		this->xdagWallet->OnMessage(eventData);
		break;
	case event_id_state_change:
		xdag_get_state_wrap();
		break;
	case event_id_state_done:
		this->xdagWallet->OnStateUpdated(eventData);
		// Should not call GetAddress at this time, since it's not loaded yet and this call would make it generate a new one.
		/*
		xdag_get_address_wrap();
		xdag_get_balance_wrap();
		*/
		break;
	case event_id_address_done:
		this->xdagWallet->OnAddressUpdated(eventData);
		break;
	case event_id_balance_done:
		this->xdagWallet->OnBalanceUpdated(eventData);
		break;
	case event_id_err_exit:
		this->xdagWallet->OnError(eve->error_no, eventData);

	default:
		break;
	}
	
	return 0;
}

void XDagRuntime::RefreshData()
{
	xdag_get_address_wrap();
	xdag_get_balance_wrap();
}

String^ XDagRuntime::ConvertFromConstChar(const char* str)
{
	std::string promptStd(str);
	return msclr::interop::marshal_as<System::String^>(promptStd);
}

const char* XDagRuntime::ConvertFromString(String^ str)
{
	std::string stdString = msclr::interop::marshal_as<std::string>(str);

	int len = strlen(stdString.c_str());

	char * result = new char[len + 1];

	strcpy_s(result, sizeof(result), stdString.c_str());


	return const_cast<char *>(result);
}