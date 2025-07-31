import { useContext } from "react"

export function getContext<T>(context: React.Context<T|undefined>) : T {
    const result = useContext(context);
    if (result === undefined) {
        throw new Error("No context provided when using "+context.displayName);
    }
    return result;
}