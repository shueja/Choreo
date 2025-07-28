import { IPathStore } from "@choreo/stores/path/PathStore";
import { createContext } from "react";

export const PathStoreContext = createContext<IPathStore|undefined>(undefined);